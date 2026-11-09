// Round twelve group BE1 (docs/takeover-queue-field-battle.md section 3;
// analysis/round12_cut.tsv's BE1 rows and the two starts the cut lacks):
// 41 functions of the battle engine's first run 0x42D7A0..0x432B6A, each
// read to its last instruction with capstone (2026-09-29) and taken through
// the boss harness's engine frame (boss_harness.h, docs/boss_harness.md
// section 10). docs/battle_e1.md has every function one row each.
//
//   - BATE (game mode 9's overlay, the byte block 0x929F00): state 3's tally
//     - six counters counted up to what 0x939A04 / 0x939A08 / 0x939A0C
//     hold, character record 7's stat words raised by them - and state 2's
//     equipment screen over the same record's two equipment bytes, with
//     their dispatchers by 0x929F01 / 0x929F02;
//   - the command menu's held-button steps (Battle_InputSteps[3] and its
//     three), auto battle (Battle_MenuSteps[5], the PSX Cmd_AutoBattle_Begin
//     by the sibling's notes, verified by reading);
//   - action kind 3's dispatcher and two steps, the random ability pick of
//     actions 0x24 / 0x25 / 0x8C, the ability notice (AfterSteps[1]);
//   - the end's party restore (0x64AF7C[2]), the result's EXP-share count,
//     zenny-bonus test, level-up notice, and a character's level gain from
//     its EXP (the result windows' and BE7's);
//   - the loss screen (0x904AE8 bit 0's way out): its dispatcher, five steps
//     and four draws.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers abort past their tables where the original jumps through
// whatever follows (round9 doc section 6); every other unchecked index is
// reproduced and described in the doc (section 7). Every call goes through
// the harness (BH_CALL / BH_AT), so the start-up fuzz can stand recorders in
// for the callees.
#include "game/battle_e1.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_e1_callees.h"
#include "game/boss_harness.h"
#include "game/labels.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = battle_e1::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& BBattleE1(U address) { return move_script::At(address)[0]; }
U LBattleE1(U address) { return static_cast<U>(move_script::Long(move_script::At(address))); }
unsigned char* PtrAtBattleE1(U address) { return move_script::At(LBattleE1(address)); }
unsigned char* Packet() { return Gfx_PacketNext; }
unsigned char* Member(U index) { return move_script::At(::bof3::Img(ObjTrio_addr) + index * at::kMemberSize); }
unsigned Pressed() { return Input_Pressed; }
// fild dword / fstp dword: the int to a float, nearest.
void PutFloat(unsigned char* p, std::int32_t v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}
void Rgb(unsigned char* p, unsigned at_, unsigned char v) { p[at_] = p[at_ + 1] = p[at_ + 2] = v; }

// jmp [table + 4 * index]: the entry as read (the fuzz swaps the table's
// cells for its recorders), a Fatal past `entries` (the original jumps
// through the dword after: docs/battle_e1.md section 7). The jmp leaves the
// caller's stack word and the entry's eax in place: ours hands the word on
// and answers the entry's eax (round 11 doc section 5.1).
using Entry = unsigned long (__cdecl*)(unsigned long);
unsigned long Dispatch(const char* who, U table, unsigned index, unsigned entries, unsigned long through) {
    if (index >= entries)
        bof3::Fatal("%s: the step byte is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/battle_e1.md section 7)",
                    who, index, entries, (unsigned)table);
    return reinterpret_cast<Entry>(static_cast<std::uintptr_t>(LBattleE1(table + 4 * index)))(through);
}

// A texture page and draw mode for the next primitives: Gpu_GetTPage(tp, abr,
// x, y), Gpu_SetDrawMode(Gfx_PacketNext, 0, 0, page & 0xFFFF, 0) - the fifth
// word is the GetTPage push the caller's `add esp, 0x10` leaves -, then
// Gfx_CommitPrim(slot, 0xC).
void DrawMode(unsigned tp, unsigned abr, int x, int y, unsigned slot) {
    const unsigned page = BH_CALL(Gpu_GetTPage)(tp, abr, x, y);
    BH_CALL(Gpu_SetDrawMode)(Packet(), 0, 0, page & 0xFFFF, 0);
    BH_CALL(Gfx_CommitPrim)(slot, 0xC);
}

// The loss screen's sprites: CLUT (0, 0x1FA), shade, x / y floats (as bit
// patterns), u, v, w, h - the fields stored before Gpu_SetSprt, as the
// original does - then Gfx_CommitPrim(2, 0x1C).
void LossSprite(unsigned char shade, U xf, U yf, unsigned char u, unsigned char v, unsigned w, unsigned h) {
    unsigned char* const p = Packet();
    auto* const sprt = rv::As<records::GpuSprt>(p);  // a SPRT: Gpu_SetSprt below starts it
    const unsigned clut = BH_CALL(Gpu_GetClut)(0, 0x1FA);
    rv::Store<unsigned>(sprt->clut, clut);
    Rgb(p, 4, shade);
    rv::Store<float>(sprt->x, std::bit_cast<float>(static_cast<std::int32_t>(xf)));
    rv::Store<float>(sprt->y, std::bit_cast<float>(static_cast<std::int32_t>(yf)));
    p[0x14] = u;
    p[0x15] = v;
    rv::Store<unsigned>(sprt->w, w);
    rv::Store<unsigned>(sprt->h, h);
    BH_CALL(Gpu_SetSprt)(p);
    BH_CALL(Gfx_CommitPrim)(2, 0x1C);
}

// A CLUT run of 0x100 words greyed: each colour's three 5-bit channels
// averaged (the sum / 3, rounded down: the original's multiply by 0x55555556)
// into all three, bit 15 kept.
void GreyClut(U at_) {
    for (unsigned k = 0; k < 0x100; ++k) {
        unsigned char* const w = At(at_ + 2 * k);
        const unsigned c = Word(w);
        const unsigned q = (((c >> 10) & 0x1F) + ((c >> 5) & 0x1F) + (c & 0x1F)) / 3;
        SetWord(w, (q << 10) | (q << 5) | q | (c & 0x8000));
    }
}

// The party's +0x130 dword (the loss steps' and the notices' flag bits).
U MemberFlags(U index) { return static_cast<U>(static_cast<std::int32_t>(rv::As<records::PartyMember>(Member(index))->flags_130)); }

}  // namespace

// ============================================================================
// BATE: state 3, the tally (0x42D760 by 0x929F01, 0x42D770 by 0x929F02)
// ============================================================================

// ============================================================================
// BATE: state 2, the equipment screen (0x42DC00 by 0x929F01)
// ============================================================================

// ============================================================================
// The command menu: the held-button steps and auto battle
// ============================================================================

// original 0x42ED90 (Battle_InputSteps[3]; PSX 0x801D22A4): by 0x904AA2 through
// BattleHold_Steps 0x64AE48 (3).
extern "C" unsigned long __cdecl BattleHold_Dispatch(unsigned long through) {
    return Dispatch("BattleHold_Dispatch", ::bof3::Img(BattleHold_Steps_addr), BattleE4_Step2, 3, through);
}

// original 0x42EDA0 (BattleHold_Steps[0]; PSX 0x801D22E0): the menu member's cross size
// 0x904ABC[0x904AB4] down by 2 while above 0, else 0x904AA2 + 1; then its
// command label, the party status at window 1's (x, y) and the command cross
// at window 2's. The original pushes the words in whole registers
// whose upper halves are left over (the callees keep the low halves: the
// fuzz compares those); ours passes the words.
extern "C" void __cdecl BattleHold_Shrink(void) {
    unsigned char& size = BBattleE1(::bof3::Img(at::kCrossGrow.v) + BBattleE1(::bof3::Img(at::kMenuMember.v)));
    if (size > 0)
        size = static_cast<unsigned char>(size - 2);
    else
        BattleE4_Step2 = static_cast<unsigned char>(BattleE4_Step2 + 1);
    BH_CALL(BattleWin_DrawCommandLabel)(BBattleE1(::bof3::Img(at::kMenuMember.v)));
    BH_CALL(BattleWin_DrawPartyStatus)(WindowRecords[1].x, WindowRecords[1].y);
    BH_CALL(BattleWin_DrawCommandCross)(WindowRecords[2].x, WindowRecords[2].y);
}

// original 0x42EE00 (BattleHold_Steps[1]; PSX 0x801D2384): while Input_Held has no 0x100,
// windows 2, 1 and 3's +3 = 1 and 0x904AA2 + 1; then the party status,
// BE4's 0x444660(), and all seven command labels with their icons
// (Menu_DrawIcon(i, 0x88 + 0x64E2AC[2i], 0x58 + 0x64E2AD[2i], 0x10, 0x10,
// 0x80)).
extern "C" void __cdecl BattleHold_ShowAll(void) {
    if (!(Input_Held & 0x100)) {
        BBattleE1(::bof3::Img(at::kWin2.v) + 3) = 1;
        BBattleE1(::bof3::Img(at::kWin1.v) + 3) = 1;
        WindowRecords[3].state = 1;
        BattleE4_Step2 = static_cast<unsigned char>(BattleE4_Step2 + 1);
    }
    BH_CALL(BattleWin_DrawPartyStatus)(WindowRecords[1].x, WindowRecords[1].y);
    BH_AT(void (__cdecl*)(), at::kBe4Draw444660.v)();
    for (unsigned i = 0; i < 7; ++i) {
        BH_CALL(BattleWin_DrawCommandLabel)(i);
        BH_CALL(Menu_DrawIcon)(i, BBattleE1(::bof3::Img(at::kHoldIcons.v) + 2 * i) + 0x88u, BBattleE1(::bof3::Img(at::kHoldIcons.v) + 2 * i + 1) + 0x58u, 0x10, 0x10, 0x80);
    }
}

// original 0x42EEA0 (BattleHold_Steps[2]; PSX 0x801D2478): the menu member's cross size up by
// 4 while below 8, else 0x904AA1 = 2 (BattleMenu_CommandSelect) and
// 0x904AA2 = 0.
extern "C" void __cdecl BattleHold_Regrow(void) {
    unsigned char& size = BBattleE1(::bof3::Img(at::kCrossGrow.v) + BBattleE1(::bof3::Img(at::kMenuMember.v)));
    if (size < 8) {
        size = static_cast<unsigned char>(size + 4);
        return;
    }
    BattleActions_Step = 2;
    BattleE4_Step2 = 0;
}

// original 0x42EF50 (Battle_MenuSteps[5]; PSX Cmd_AutoBattle_Begin
// 0x801D2598 by the sibling's names/functions.toml, read and agreeing):
// window 4's +3 = 2; each of the s8 0x904AC3 entries 0x904AB6[i] (the
// count re-read each pass) not 0xFF and not out whose command +0x125 is 5
// (an item) - an entry is indexed into ObjTrio unchecked -, BE4's
// 0x446D90(+0x12E, word +0x126) gives the item back; then each member not
// out (0x904AB0 of them, re-read) - with 0x904B8E set only one whose +0x134
// has 0x10 - goes to state 2 (+1); round flags |= 0x10, LoadDatFile(0xD1),
// 0x904AA1 = 0x904AA2 = 0.
extern "C" void __cdecl Cmd_AutoBattle(void) {
    const auto entries = BattleActorCopies_EntryCount;
    WindowRecords[4].state = 2;
    if (entries > 0) {
        unsigned i = 0;
        do {
            const unsigned char actor = BBattleE1(::bof3::Img(at::kEntryFirst.v) + i);
            if (actor != 0xFF && !BH_CALL(Battle_ActorIsOut)(actor)) {
                unsigned char* const m = Member(actor);
                if (m[0x125] == 5)
                    BH_AT(unsigned char (__cdecl*)(unsigned, unsigned), at::kBe4ReturnItem.v)(m[0x12E], rv::As<records::PartyMember>(m)->queued_id);
            }
            i = (i + 1) & 0xFF;
        } while (static_cast<int>(i) < BattleActorCopies_EntryCount);
    }
    if (BBattleE1(::bof3::Img(at::kPartyCount.v)) != 0) {
        unsigned char i = 0;
        do {
            if (!BH_CALL(Battle_ActorIsOut)(i)) {
                if (BattleFlow_AnimGate == 0 || (Member(i)[0x134] & 0x10)) rv::As<records::PartyMember>(Member(i))->state = 2;
            }
            ++i;
        } while (i < BBattleE1(::bof3::Img(at::kPartyCount.v)));
    }
    BBattleE1(::bof3::Img(at::kFlags.v)) = static_cast<unsigned char>(BBattleE1(::bof3::Img(at::kFlags.v)) | 0x10);
    BH_CALL(LoadDatFile)(0xD1);
    BattleActions_Step = 0;
    BattleE4_Step2 = 0;
}

// ============================================================================
// Action kind 3, the random pick, the ability notice
// ============================================================================

// original 0x42F5E0 (BattleAction_KindSteps[3]; PSX 0x801D31B8): by 0x904AA3 through
// BattleAction_Kind3Steps 0x64AEB4 (5: the two below, then
// BattleAction_AbilitySteps' three).
extern "C" unsigned long __cdecl BattleAction_Kind3Dispatch(unsigned long through) {
    return Dispatch("BattleAction_Kind3Dispatch", ::bof3::Img(BattleAction_Kind3Steps_addr), BattleE4_Step3, 5, through);
}

// original 0x42F5F0 (Kind3Steps[0]; PSX 0x801D31F4): Battle_ClearActingFlags;
// BattleBanner_Add(1, 0, 0, 0x1E, the string 0x669E08 points at); window 4's
// +3 = 1; Battle_SetActorBit(0x904B34); the acting sprite's +1 = 9;
// 0x904AA3 + 1.
extern "C" void __cdecl BattleAction_Kind3Banner(void) {
    BH_CALL(Battle_ClearActingFlags)();
    BH_CALL(BattleBanner_Add)(1, 0, 0, 0x1E, reinterpret_cast<const char*>(BattleE1_BannerText.Native()));
    const unsigned char actor = BBattleE1(::bof3::Img(at::kActor.v));
    WindowRecords[4].state = 1;
    BH_CALL(Battle_SetActorBit)(actor);
    static_cast<unsigned char*>(MagicS23_ActorSprite)[1] = 9;
    BattleE4_Step3 = static_cast<unsigned char>(BattleE4_Step3 + 1);
}

// original 0x42F640 (Kind3Steps[1]; PSX 0x801D3278): once the word 0x904B82 is 0, round
// flags |= 4, 0x904AA1 = 3, 0x904AA2 = 0x904AA3 = 0.
extern "C" void __cdecl BattleAction_Kind3Wait(void) {
    if (BattleActions_Pending != 0) return;
    BBattleE1(::bof3::Img(at::kFlags.v)) = static_cast<unsigned char>(BBattleE1(::bof3::Img(at::kFlags.v)) | 4);
    BattleActions_Step = 3;
    BattleE4_Step2 = 0;
    BattleE4_Step3 = 0;
}

// original 0x42F9D0: for the action word 0x904B80 0x24, an id from
// 0x64AEC8[Rand() & 0x1F]; for 0x25 or 0x8C (the word re-read after that
// Rand), from 0x64AEE8[Rand() & 0x1F]; any other gives 0. The id to
// 0x904B80 and the action record's (0x904B40, read first) +2; then by the
// id's flags byte (NameTable_Abilities + 24 id): 0x10 - 0xC0 for 0x80
// without 0x40, else a side by the actor (party: 0x40 with 0x20, 0x80
// without; enemy: 0x80 with, 0x40 without); no 0x10 and no 0x40 - the actor
// 0x904B34; 0x40 - 0x452F10() or 0x452EB0() by the actor's side and 0x20.
// al out.
extern "C" unsigned char __cdecl BattleAction_PickRandomAbility(void) {
    unsigned char id = 0;
    unsigned action = move_script::Word(move_script::At(::bof3::Img(at::kActionId.v)));
    if (action == 0x24) {
        id = BBattleE1(::bof3::Img(at::kRandomIds24.v) + (static_cast<U>(BH_CALL(Rand)()) & 0x1F));
        action = move_script::Word(move_script::At(::bof3::Img(at::kActionId.v)));
    }
    if (action == 0x25 || action == 0x8C) id = BBattleE1(::bof3::Img(at::kRandomIds25.v) + (static_cast<U>(BH_CALL(Rand)()) & 0x1F));
    unsigned char* const record = PtrAtBattleE1(::bof3::Img(at::kActing2.v));
    move_script::SetWord(move_script::At(::bof3::Img(at::kActionId.v)), id);
    const unsigned char flags = BBattleE1(::bof3::Img(bof3::addr::NameTable_Abilities) + 24u * id);
    rv::Store<unsigned>(rv::As<records::ActionRecord>(record)->action_id, id);  // 0x904B40: the acting block's action record
    const bool party = BBattleE1(::bof3::Img(at::kActor.v)) <= 2;
    if (flags & 0x10) {
        if ((flags & 0x80) && !(flags & 0x40)) return 0xC0;
        if (party) return (flags & 0x20) ? 0x40 : 0x80;
        return (flags & 0x20) ? 0x80 : 0x40;
    }
    if (!(flags & 0x40)) return BBattleE1(::bof3::Img(at::kActor.v));
    const bool second = party ? (flags & 0x20) != 0 : (flags & 0x20) == 0;
    return BH_WORD(unsigned char (__cdecl*)(), second ? at::kSideTargetB.v : at::kSideTargetA.v)();
}

// original 0x42FE20 (BattleAction_AfterSteps[1]): nothing while the message
// window is up (0x939F60); else from the party slot after 0x904AA5 (stored
// as it goes; none left past slot 2: 0x904AA2 + 1) the first member whose
// +0x130 has bit 3: the bit cleared, and by the action's ability (the
// record 0x904B40's word +2) flag 0x200 in Ability_Records +0x14 - BE4's
// 0x44A910(member) (the name into Text_Records), Text_Records + 0x20 zeroed
// (32 bytes) and the acting enemy's ability record (+0x106 of enemy
// 0x904B34 - 3, unchecked) copied in (16 bytes), BattleQueue_Push(1, 0,
// message 0x24); without it, BattleQueue_Push(1, 0, message 0x23).
extern "C" void __cdecl BattleAction_AbilityNotice(void) {
    if (BattleE1_MessageUp != 0) return;
    unsigned char slot = BossSb_Countdown;
    if (slot <= 2) {
        const unsigned char* const record = PtrAtBattleE1(::bof3::Img(at::kActing2.v));
        do {
            ++slot;
            BossSb_Countdown = slot;
            const U id = rv::As<records::ActionRecord>(record)->action_id;  // 0x904B40: the acting block's action record
            const bool learns = (move_script::Word(move_script::At(::bof3::Img(0x65C4DC) + 24 * id)) & 0x200) != 0;
            unsigned char* const flags = move_script::At(::bof3::Img(0x802D24) + slot * at::kMemberSize);
            if (!(flags[0] & 8)) continue;
            move_script::SetLong(flags, static_cast<std::int32_t>(static_cast<U>(move_script::Long(flags)) & ~8u));
            if (!learns) {
                BH_CALL(BattleQueue_Push)(1, 0, Low32(BH_CALL(Msg_SystemPtr)(0x23)));
                return;
            }
            BH_AT(void (__cdecl*)(unsigned), at::kBe4MemberName.v)(slot - 1u);
            std::memset(move_script::At(::bof3::Img(bof3::addr::Text_Records) + 0x20), 0, 32);
            const U enemy = static_cast<U>(BBattleE1(::bof3::Img(at::kActor.v))) - 3;
            const U ability = move_script::Word(move_script::At(::bof3::Img(0x93BA66) + enemy * 0x128));
            const U from = ::bof3::Img(bof3::addr::Ability_Records) + 24 * ability;
            move_script::SetLong(move_script::At(::bof3::Img(bof3::addr::Text_Records) + 0x20), move_script::Long(move_script::At(from)));
            move_script::SetLong(move_script::At(::bof3::Img(bof3::addr::Text_Records) + 0x24), move_script::Long(move_script::At(from + 4)));
            move_script::SetLong(move_script::At(::bof3::Img(bof3::addr::Text_Records) + 0x28), move_script::Long(move_script::At(from + 8)));
            move_script::SetLong(move_script::At(::bof3::Img(bof3::addr::Text_Records) + 0x2C), move_script::Long(move_script::At(from + 0xC)));
            BH_CALL(BattleQueue_Push)(1, 0, Low32(BH_CALL(Msg_SystemPtr)(0x24)));
            return;
        } while (slot <= 2);
    }
    BattleE4_Step2 = static_cast<unsigned char>(BattleE4_Step2 + 1);
}

// ============================================================================
// The end and the result
// ============================================================================

// original 0x4315C0 (0x64AF7C[2], the third way out's steps by 0x904AA2; PSX 0x801D6EF8):
// nothing while a member (of 0x904AB0, read once) has 0x2000 in its +0x130;
// back a step (0x904AA2 - 1) while 0x904AA5 is short of the count; then
// Port_DroppedCall(0); each member Battle_ClearStatus(i, word +0x90 &
// 0xBF5F) and Sprite_ReleaseTint; each member with +0 bit 0, as
// Sprite_Current, Sprite_AnimFromSet(+0x4B, word +0x58 - 2, 0x8C5D80,
// 0x1800) - the count re-read after each of those calls only -;
// PartySet_Select(0x90412C & 0x7F, 0); 0x904AA2 + 1.
extern "C" void __cdecl BattleEnd_AwaitRestore(void) {
    const unsigned char n = BBattleE1(::bof3::Img(at::kPartyCount.v));
    for (unsigned char i = 0; i < n; ++i)
        if (MemberFlags(i) & 0x2000) return;
    if (BossSb_Countdown != n) {
        BattleE4_Step2 = static_cast<unsigned char>(BattleE4_Step2 - 1);
        return;
    }
    BH_CALL(Port_DroppedCall)(0);
    unsigned char count = BBattleE1(::bof3::Img(at::kPartyCount.v));
    for (unsigned char i = 0; i < count;) {
        unsigned char* const m = Member(i);
        auto* const member_rec = rv::As<records::PartyMember>(m);
        BH_CALL(Battle_ClearStatus)(i, member_rec->status & 0xBF5Fu);
        BH_CALL(Sprite_ReleaseTint)(m);
        count = BBattleE1(::bof3::Img(at::kPartyCount.v));
        ++i;
    }
    for (unsigned char i = 0; i < count;) {
        unsigned char* const m = Member(i);
        auto* const member_rec = rv::As<records::PartyMember>(m);
        if (member_rec->flags & 1) {
            Sprite_Current = m;
            BH_CALL(Sprite_AnimFromSet)(member_rec->animation, static_cast<std::uint16_t>(member_rec->script_pos - 2), move_script::At(::bof3::Img(at::kAnimBuffer.v)), 0x1800);
            count = BBattleE1(::bof3::Img(at::kPartyCount.v));
        }
        ++i;
    }
    BH_CALL(PartySet_Select)(AreaW0b_PartySet & 0x7Fu, 0);
    BattleE4_Step2 = static_cast<unsigned char>(BattleE4_Step2 + 1);
}

// original 0x4319B0: of the 0x904AB0 party slots (re-read each pass), those
// Battle_ActorIsOut answers 0 for whose +0x134 lacks 0x400: the count, al.
// BattleResult_SplitExp's divisor.
extern "C" unsigned char __cdecl BattleResult_CountExpShares(void) {
    unsigned char count = 0;
    if (BBattleE1(::bof3::Img(at::kPartyCount.v)) == 0) return 0;
    unsigned char i = 0;
    do {
        if (!BH_CALL(Battle_ActorIsOut)(i) && !(static_cast<std::int32_t>(rv::As<records::PartyMember>(Member(i))->flags_134) & 0x400)) ++count;
        ++i;
    } while (i < BBattleE1(::bof3::Img(at::kPartyCount.v)));
    return count;
}

// original 0x431C10 (BattleResult_LevelUpSteps[1]): nothing until a button
// is pressed; then BE4's 0x44A910(0x904AA5 - 1) (the name), the old level
// (record 0x904AA7's +0xA) to 0x904B72, the new one (Char_LevelUpGain(roster,
// 0) + the level, the roster re-read after) to 0x904B74 and through
// Crt_sprintf into Text_Records + 0x20; 0x93B8E4 = message 7; the window's
// rows: 0x10 for each stat 1..6 Char_LevelUpGain answers non-zero for, and
// for each of the bytes +6 / +7 of the old level's Char_ExpTable row;
// Window_Alloc(1, 4); window 1's +0xA = the roster, +2 = +3 = 0, +9 = the
// rows + 4, +0xB = the old level; 0x904AA4 - 1 (back to the search).
extern "C" void __cdecl BattleResult_LevelUpNotice(void) {
    if (Pressed() == 0) return;
    BH_AT(void (__cdecl*)(unsigned), at::kBe4MemberName.v)(static_cast<unsigned char>(BossSb_Countdown - 1));
    const unsigned char roster = BattleE1_Roster;
    SetWord(At(::bof3::Img(at::kOldLevel.v)), BBattleE1(::bof3::Img(at::kCharRecords.v) + roster * at::kCharSize + 0xA));
    const unsigned gain = BH_CALL(Char_LevelUpGain)(roster, 0);
    const unsigned char again = BattleE1_Roster;
    SetWord(At(::bof3::Img(at::kNewLevel.v)), (gain & 0xFFFF) + BBattleE1(::bof3::Img(at::kCharRecords.v) + again * at::kCharSize + 0xA));
    BH_CALL(Crt_sprintf)(reinterpret_cast<char*>(At(::bof3::Img(bof3::addr::Text_Records) + 0x20)),
                         reinterpret_cast<const char*>(At(::bof3::Img(bof3::addr::Area08_MessageFormat))), Word(At(::bof3::Img(at::kNewLevel.v))));
    SetLong(At(::bof3::Img(at::kLevelMsgPtr.v)), static_cast<std::int32_t>(Low32(BH_CALL(Msg_SystemPtr)(7))));
    U rows = 0;
    for (unsigned k = 1; k <= 6; ++k)
        if ((BH_CALL(Char_LevelUpGain)(BattleE1_Roster, k) & 0xFFFF) != 0) rows += 0x10;
    const U row = ::bof3::Img(at::kCharExpTable.v) + (BattleE1_Roster * 99u + Word(At(::bof3::Img(at::kOldLevel.v)))) * 8;
    if (BBattleE1(row + 6) != 0) rows += 0x10;
    if (BBattleE1(row + 7) != 0) rows += 0x10;
    BH_CALL(Window_Alloc)(1, 4);
    const unsigned char who = BattleE1_Roster;
    const unsigned char old = BBattleE1(::bof3::Img(at::kOldLevel.v));
    BBattleE1(::bof3::Img(at::kWin1.v) + 0xA) = who;
    const unsigned char step = BattleE4_Step4;
    BBattleE1(::bof3::Img(at::kWin1.v) + 2) = 0;
    BBattleE1(::bof3::Img(at::kWin1.v) + 3) = 0;
    BBattleE1(::bof3::Img(at::kWin1.v) + 9) = static_cast<unsigned char>(rows + 4);
    BBattleE1(::bof3::Img(at::kWin1.v) + 0xB) = old;
    BattleE4_Step4 = static_cast<unsigned char>(step - 1);
}

// original 0x431FE0: 1 when a party slot (of 0x904AB0, re-read) that
// Battle_ActorIsOut answers 0 for holds 7 in its +0x96 or +0x97; else 0. al.
extern "C" unsigned char __cdecl BattleResult_ZennyBonus(void) {
    if (BBattleE1(::bof3::Img(at::kPartyCount.v)) == 0) return 0;
    unsigned char i = 0;
    do {
        if (!BH_CALL(Battle_ActorIsOut)(i)) {
            auto* const member_rec = rv::As<records::PartyMember>(Member(i));
            if (member_rec->accessory[0] == 7 || member_rec->accessory[1] == 7) return 1;
        }
        ++i;
    } while (i < BBattleE1(::bof3::Img(at::kPartyCount.v)));
    return 0;
}

// original 0x432170 (PSX 0x801EF92C by the catalogue's pair; the sibling's
// name there is an unrelated overlay's): the level character record
// `roster`'s EXP (+0xC, signed) reaches - the first L of 1..98 whose
// Char_ExpTable words (the roster's 99 rows of 8) summed over 1..L exceed
// it, else 99 - and by `what`: 0 the levels gained over its level +0xA (0 if
// none); 1..6 the gain of one stat over those levels (the rows from the
// level on: 1 byte +2, 2 byte +3, 3 byte +4's high nibble, 4 its low, 5 byte
// +5's high, 6 its low; each plus the record's s8 +0x89..+0x8E, summed in 16
// bits, 0 if negative, 0 with no level gained); anything else 0. ax out.
// Neither byte is checked.
extern "C" unsigned short __cdecl Char_LevelUpGain(unsigned roster_arg, unsigned what_arg) {
    const U roster = roster_arg & 0xFF;
    const U base = roster * 99;
    const unsigned char* const record = At(::bof3::Img(CharacterRecords_addr) + roster * at::kCharSize);
    static_assert(at::kCharSize == sizeof(records::CharRecord));
    const unsigned char level = record[0xA];
    unsigned char reached = 1;
    U sum = 0;
    for (;;) {
        sum += Word(At(::bof3::Img(at::kCharExpTable.v) + (base + reached) * 8));
        if (static_cast<std::int32_t>(sum) > rv::As<records::CharRecord>(record)->exp) break;
        ++reached;
        if (reached >= 0x63) break;
    }
    const U what = what_arg & 0xFF;
    if (what > 6) return 0;
    if (what == 0) {
        const std::int32_t gained = static_cast<std::int32_t>(reached) - static_cast<std::int32_t>(level);
        return static_cast<unsigned short>(gained > 0 ? gained : 0);
    }
    if (level >= reached) return 0;
    struct Growth { unsigned char stat, byte; int nibble; };   // nibble: 0 the byte, 1 high, 2 low
    static const Growth kGrowth[7] = {{0, 0, 0}, {0x89, 2, 0}, {0x8A, 3, 0}, {0x8B, 4, 1}, {0x8C, 4, 2}, {0x8D, 5, 1}, {0x8E, 5, 2}};
    const Growth& g = kGrowth[what];
    const auto bonus = static_cast<std::uint16_t>(static_cast<std::int16_t>(static_cast<signed char>(record[g.stat])));
    std::uint16_t total = 0;
    for (U lv = level; lv < reached; ++lv) {
        const unsigned char b = BBattleE1(::bof3::Img(at::kCharExpTable.v) + (base + lv) * 8 + g.byte);
        const unsigned v = g.nibble == 1 ? b >> 4 : g.nibble == 2 ? b & 0xF : b;
        total = static_cast<std::uint16_t>(total + v + bonus);
    }
    return static_cast<std::int16_t>(total) < 0 ? 0 : total;
}

// ============================================================================
// The loss screen (0x904AE8 bit 0's way out: BattleEnd_Steps[2], its table
// 0x64AF70 by 0x904AA2, entry 2)
// ============================================================================

// original 0x432430 (0x64AF70[2]): by 0x904AA3 through BattleLoss_Steps
// 0x64AFD8 (5).
extern "C" unsigned long __cdecl BattleLoss_Dispatch(unsigned long through) {
    return Dispatch("BattleLoss_Dispatch", ::bof3::Img(BattleLoss_Steps_addr), BattleE4_Step3, 5, through);
}

// original 0x432440 (BattleLoss_Steps[0]): Transition_Start(0x13),
// Gfx_ClutStripCopyRow(0x1A), 0x904AA3 + 1.
extern "C" void __cdecl BattleLoss_FadeOut(void) {
    BH_CALL(Transition_Start)(0x13);
    BH_CALL(Gfx_ClutStripCopyRow)(0x1A);
    BattleE4_Step3 = static_cast<unsigned char>(BattleE4_Step3 + 1);
}

// The loss screen's member reset: Sprite_Current the member,
// Sprite_EnsureAnimation(+8 + 0x1C), +0x29 = 1, +0x24 |= 0x80 (read after
// the call), +0x2E / +0x30 the words of BattleLoss_Steps' neighbour rows
// (0x64AFD8 + 24 * the count, re-read, + 8 * the member) - data read in
// place, past the five code pointers -, Sprite_ReleaseTint(member).
namespace {
void LossMember(U index) {
    unsigned char* const m = Member(index);
    auto* const member_rec = rv::As<records::PartyMember>(m);
    Sprite_Current = m;
    BH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(member_rec->direction + 0x1C));
    const auto flags = static_cast<unsigned char>(m[0x24] | 0x80);
    member_rec->ot_slot = 1;
    m[0x24] = flags;
    const U row = ::bof3::Img(BattleLoss_Steps_addr) + 24 * static_cast<U>(BBattleE1(::bof3::Img(at::kPartyCount.v))) + 8 * index;
    member_rec->screen_x = Word(At(row));
    member_rec->screen_y = Word(At(row + 4));
    BH_CALL(Sprite_ReleaseTint)(m);
}
}  // namespace

// original 0x432460 (BattleLoss_Steps[1]): once no transition runs, window
// 0's +0 = 0; members 0, 1, 2 reset (LossMember) while the count (re-read)
// is at least 1, 2, exactly 3; then 0x494E70() (the enemies' state bytes),
// BattleTask_ClearAll, Transition_Start(0xE), 0x904AA3 + 1.
extern "C" void __cdecl BattleLoss_ResetParty(void) {
    if (Transition_Busy != 0) return;
    const unsigned char n = BBattleE1(::bof3::Img(at::kPartyCount.v));
    WindowRecords[0].in_use = 0;
    if (n >= 1) {
        LossMember(0);
        if (BBattleE1(::bof3::Img(at::kPartyCount.v)) >= 2) {
            LossMember(1);
            if (BBattleE1(::bof3::Img(at::kPartyCount.v)) == 3) LossMember(2);
        }
    }
    BH_AT(void (__cdecl*)(), at::kEnemiesClear.v)();
    BH_CALL(BattleTask_ClearAll)();
    BH_CALL(Transition_Start)(0xE);
    BattleE4_Step3 = static_cast<unsigned char>(BattleE4_Step3 + 1);
}

// original 0x4325F0 (BattleLoss_Steps[2]): the black screen and the panels
// (shade 0x80); once no transition runs, the bar 0x904B70 = 0x2C and
// 0x904AA3 + 1.
extern "C" void __cdecl BattleLoss_Show(void) {
    BH_CALL(BattleLoss_DrawBlack)();
    BH_CALL(BattleLoss_DrawPanels)(0x80);
    if (Transition_Busy != 0) return;
    BattleResult_TickStep = 0x2C;
    BattleE4_Step3 = static_cast<unsigned char>(BattleE4_Step3 + 1);
}

// original 0x432630 (BattleLoss_Steps[3]): the black screen, the panels, the
// caption; the bar up by 4 while below 0xF4, drawn; on any held button both
// CLUT runs 0x812980 / 0x811380 greyed, Gfx_ClutStripDirty = 1,
// Transition_Start(0xD), 0x904AA3 + 1.
extern "C" void __cdecl BattleLoss_BarGrow(void) {
    BH_CALL(BattleLoss_DrawBlack)();
    BH_CALL(BattleLoss_DrawPanels)(0x80);
    BH_CALL(BattleLoss_DrawCaption)(0x80);
    unsigned width = BattleResult_TickStep;
    if (width < 0xF4) {
        width += 4;
        BattleResult_TickStep = width;
    }
    BH_CALL(BattleLoss_DrawBar)(width);
    if (Input_Held == 0) return;
    GreyClut(::bof3::Img(Gfx_ClutStrip_addr) + 0x3400);
    GreyClut(::bof3::Img(Gfx_ClutStrip_addr) + 0x1e00);
    Gfx_ClutStripDirty = 1;
    BH_CALL(Transition_Start)(0xD);
    BattleE4_Step3 = static_cast<unsigned char>(BattleE4_Step3 + 1);
}

// original 0x432750 (BattleLoss_Steps[4]): the black screen, the panels, the
// caption; once no transition runs, the three members' tints released,
// Window_ResetAll, 0x904AE9 / 0x904AE5 / the fight byte / the phase and its
// four step bytes 0, 0x494E70(), BattleTask_ClearAll, Sound_StopChannels,
// Task_Restart(Boot_Task) (which does not return in the game).
extern "C" void __cdecl BattleLoss_Restart(void) {
    BH_CALL(BattleLoss_DrawBlack)();
    BH_CALL(BattleLoss_DrawPanels)(0x80);
    BH_CALL(BattleLoss_DrawCaption)(0x80);
    if (Transition_Busy != 0) return;
    BH_CALL(Sprite_ReleaseTint)(Member(0));
    BH_CALL(Sprite_ReleaseTint)(Member(1));
    BH_CALL(Sprite_ReleaseTint)(Member(2));
    BH_CALL(Window_ResetAll)();
    BattleMisc_BannerKinds = 0;
    EventBattle_Flags = 0;
    EventBattle_Id = 0;
    BBattleE1(::bof3::Img(0x904AA0)) = 0;
    BattleActions_Step = 0;
    BattleE4_Step2 = 0;
    BattleE4_Step3 = 0;
    BattleE4_Step4 = 0;
    BH_AT(void (__cdecl*)(), at::kEnemiesClear.v)();
    BH_CALL(BattleTask_ClearAll)();
    BH_CALL(Sound_StopChannels)();
    BH_CALL(Task_Restart)(reinterpret_cast<void*>(static_cast<std::uintptr_t>(::bof3::Code(bof3::addr::Boot_Task))));
}

// original 0x4327F0: three SPRTs of page (0x340, 0x100) and CLUT (0, 0x1FA)
// in the shade byte: 0x100 x 0xA0 from (0, 0x60) at (16, 60); 0x20 x 0x60
// from (0xC0, 0) at (272, 60); 0x20 x 0x40 from (0xE0, 0) at (272, 156).
extern "C" void __cdecl BattleLoss_DrawPanels(unsigned shade) {
    DrawMode(1, 0, 0x340, 0x100, 2);
    const auto s = static_cast<unsigned char>(shade);
    LossSprite(s, 0x41800000u, 0x42700000u, 0, 0x60, 0x100, 0xA0);
    LossSprite(s, 0x43880000u, 0x42700000u, 0xC0, 0, 0x20, 0x60);
    LossSprite(s, 0x43880000u, 0x431C0000u, 0xE0, 0, 0x20, 0x40);
}

// original 0x432930: page (0x3C0, 0), then a black TILE over (0, 0) ..
// (320, 240), Gfx_CommitPrim(2, 0x1C).
//
// DIV-0041: under a wide picture the black covers it whole, (-53, 0) 426 x
// 240, as the fade tile and the save menu's black do - the owner's shot,
// 2026-09-30: the battle field showing in both bands beside GAME OVER.
// Widescreen_Fill is 0 until every self-test has run (widescreen.h).
extern "C" void __cdecl BattleLoss_DrawBlack(void) {
    DrawMode(0, 0, 0x3C0, 0, 2);
    unsigned char* const p = Packet();
    auto* const tile = rv::As<records::GpuTile>(p);  // a TILE: Gpu_SetTile
    BH_CALL(Gpu_SetTile)(p);
    rv::Store<float>(tile->x, Widescreen_FillX());
    rv::Store<float>(tile->y, std::bit_cast<float>(0));
    rv::Store<float>(tile->w, Widescreen_FillWidth());   // 0x43A00000 narrow
    rv::Store<float>(tile->h, std::bit_cast<float>(0x43700000));
    Rgb(p, 4, 0);
    BH_CALL(Gfx_CommitPrim)(2, 0x1C);
}

// original 0x4329A0: page (0x340, 0x100), one SPRT 0xA8 x 0x18 from (0, 0)
// at (76, 48) in the shade byte, CLUT (0, 0x1FA).
extern "C" void __cdecl BattleLoss_DrawCaption(unsigned shade) {
    DrawMode(1, 0, 0x340, 0x100, 2);
    LossSprite(static_cast<unsigned char>(shade), 0x42980000u, 0x42400000u, 0, 0, 0xA8, 0x18);
}

// original 0x432A30: the bar of the low word w: page (0x3C0, 0), a black
// TILE at (w + 0x20, 48) of 168 x 24 (Gfx_CommitPrim(1, 0x1C)); page
// (0x3C0, 0) with abr 2, a semi-transparent POLY_G4 (w, 48) .. (w + 0x22,
// 72), black on the left, white on the right (Gfx_CommitPrim(1, 0x44)).
extern "C" void __cdecl BattleLoss_DrawBar(unsigned width) {
    const auto w = static_cast<std::int32_t>(width & 0xFFFF);
    DrawMode(0, 0, 0x3C0, 0, 1);
    unsigned char* p = Packet();
    BH_CALL(Gpu_SetTile)(p);
    SetLong(p + 0xC, 0x42400000);
    SetLong(p + 0x14, 0x43280000);
    SetLong(p + 0x18, 0x41C00000);
    PutFloat(p + 8, w + 0x20);
    Rgb(p, 4, 0);
    BH_CALL(Gfx_CommitPrim)(1, 0x1C);
    DrawMode(0, 2, 0x3C0, 0, 1);
    p = Packet();
    BH_CALL(Gpu_SetPolyG4)(p);
    Rgb(p, 4, 0);
    PutFloat(p + 8, w);
    Rgb(p, 0x14, 0xFF);
    Rgb(p, 0x24, 0);
    Rgb(p, 0x34, 0xFF);
    SetLong(p + 0xC, 0x42400000);
    PutFloat(p + 0x18, w + 0x22);
    PutFloat(p + 0x28, w);
    SetLong(p + 0x1C, 0x42400000);
    SetLong(p + 0x2C, 0x42900000);
    PutFloat(p + 0x38, w + 0x22);
    SetLong(p + 0x3C, 0x42900000);
    BH_CALL(Gpu_SetSemiTrans)(p, 1);
    BH_CALL(Gfx_CommitPrim)(1, 0x44);
}

// ============================================================================

void BattleE1_Inject() {
    if (bof3::WantsShadow("battle_e1")) battle_e1::SelfTest();
    BOF3_INJECT(BattleHold_Dispatch);
    BOF3_INJECT(BattleHold_Shrink);
    BOF3_INJECT(BattleHold_ShowAll);
    BOF3_INJECT(BattleHold_Regrow);
    BOF3_INJECT(Cmd_AutoBattle);
    BOF3_INJECT(BattleAction_Kind3Dispatch);
    BOF3_INJECT(BattleAction_Kind3Banner);
    BOF3_INJECT(BattleAction_Kind3Wait);
    BOF3_INJECT(BattleAction_PickRandomAbility);
    BOF3_INJECT(BattleAction_AbilityNotice);
    BOF3_INJECT(BattleEnd_AwaitRestore);
    BOF3_INJECT(BattleResult_CountExpShares);
    BOF3_INJECT(BattleResult_LevelUpNotice);
    BOF3_INJECT(BattleResult_ZennyBonus);
    BOF3_INJECT(Char_LevelUpGain);
    BOF3_INJECT(BattleLoss_Dispatch);
    BOF3_INJECT(BattleLoss_FadeOut);
    BOF3_INJECT(BattleLoss_ResetParty);
    BOF3_INJECT(BattleLoss_Show);
    BOF3_INJECT(BattleLoss_BarGrow);
    BOF3_INJECT(BattleLoss_Restart);
    BOF3_INJECT(BattleLoss_DrawPanels);
    BOF3_INJECT(BattleLoss_DrawBlack);
    BOF3_INJECT(BattleLoss_DrawCaption);
    BOF3_INJECT(BattleLoss_DrawBar);
}

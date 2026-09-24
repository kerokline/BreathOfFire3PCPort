// Battle set-up: group BA of the seventh round, nineteen functions of the
// battle engine (PC 0x42E400..0x4551A0, the PSX's BATTLE.EMI compiled into
// the exe), each read whole against its PSX twin. docs/battle_setup.md.
//
//   - The per-turn status chain: the sub-state machine 0x430510 (not ours)
//     calls nine steps one per state, stopping at the first that answers 1
//     (it showed something): BattleStep_Expire4000 0x430640, _Restore800
//     0x430790, _PartyWake40 0x430890, _EnemyWake40 0x430970, _PartyWake20
//     0x430A50, _EnemyWake20 0x430B30, _Status80 0x430C10, _HpDrift 0x430D40,
//     _ApUpkeep 0x430F40 (PSX 0x801D524C .. 0x801D62A8, the same order).
//     Each skips an actor Battle_ActorSkipped 0x431030 names.
//   - Battle_TickCounters 0x4303D0 (the counters those steps test), and
//     Battle_ClearActingFlags 0x4301B0 (the acting actor's flags 0x40 / 0x80).
//   - Battle_MarkFasterSide 0x4453C0 with its tests Battle_ActorStanding
//     0x445550 and Battle_PartyOutpaces 0x4455C0 (PSX 0x801DB020 ..).
//   - Battle_ClearStatus 0x44F4B0 (PSX 0x800A0680), Battle_OpenMsgWindow
//     0x444310, Battle_ReturnQueuedItem 0x446EA0, Battle_OpeningMessage
//     0x44AA00.
//
// Every call goes through battle_setup::g (battle_setup_callees.h), so that the
// start-up fuzz can stand recorders in for them - for ours and for the
// originals' copies alike.
#include "game/battle_setup.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_setup_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"

namespace battle_setup {

namespace {
template <typename F> F Fn(std::uint32_t address) { return reinterpret_cast<F>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    Fn<unsigned (__cdecl*)(unsigned)>(kActorAbsent),
    Fn<unsigned (__cdecl*)(unsigned)>(kActorCanAct),
    Fn<void (__cdecl*)(unsigned)>(kSetPending),
    Fn<void (__cdecl*)(unsigned, unsigned, unsigned, unsigned, const unsigned char*)>(kMessage),
    Fn<void (__cdecl*)(unsigned, unsigned, unsigned, unsigned, unsigned, const unsigned char*)>(kMessageAt),
    Fn<void (__cdecl*)(unsigned)>(kStatusTint),
    Fn<void (__cdecl*)(unsigned)>(kPartyName),
    Fn<void (__cdecl*)(unsigned)>(kEnemyName),
    Fn<unsigned (__cdecl*)(unsigned)>(kWakeRoll),
    Fn<void (__cdecl*)(unsigned)>(kSetHpChange),
    Fn<unsigned (__cdecl*)(unsigned, unsigned, unsigned)>(kEnemyOutpaces),
    Fn<unsigned (__cdecl*)(unsigned, unsigned)>(kReturnItem),
    Sprite_ReleaseTint,
    Msg_SystemPtr,
    Window_Alloc,
    Rand,
    Battle_ActorSkipped,
    Battle_OpenMsgWindow,
    Battle_ClearStatus,
    Battle_ActorStanding,
    Battle_PartyOutpaces,
};
Callees g = kOriginals;

}  // namespace battle_setup

using namespace battle_setup;

namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char* P(unsigned actor) { return At(at::Party(actor)); }
unsigned char* E(unsigned actor) { return At(at::Enemy(actor)); }
bool Yes(unsigned al) { return (al & 0xFFu) != 0; }

// The state bytes an effect sets, in the originals' order: +1 = 6, +2 = 5,
// +4 = 0, +3 = 0.
void MarkEffect(unsigned char* o) {
    o[at::kState1] = 6;
    o[at::kState2] = 5;
    o[at::kState4] = 0;
    o[at::kState3] = 0;
}

// The message every step ends with: Battle_OpenMsgWindow, then the line's
// text, then the line (2, 0, 0, 0x2D, text).
void ShowLine(const unsigned char* text) { g.message(2, 0, 0, 0x2D, text); }

}  // namespace

// ===========================================================================
// The small helpers

// original 0x431030 (PSX 0x801D6494, the same): 1 when the actor is left out
// of the chain's steps - while the timer 0x904B8E runs, any actor without
// flag 0x10 (party +0x134, enemy +0x114); otherwise when 0x4456C0 says so
// (not present, or status 0x4000). The actor is the argument's low byte; the
// whole argument goes to 0x4456C0, which masks it. The answer is al (every
// caller, all in this file, tests al alone).
extern "C" unsigned __cdecl Battle_ActorSkipped(unsigned actor) {
    const unsigned a = actor & 0xFF;
    if (At(at::kTimer)[0] != 0) {
        const unsigned char flags = a <= 2 ? P(a)[at::kPFlags] : E(a)[at::kEFlags];
        if ((flags & 0x10) == 0) return 1;
    }
    return Yes(g.actor_absent(actor)) ? 1 : 0;
}

// original 0x444310 (PSX 0x801D9454, the same): Window_Alloc(0, 3), then the
// four fields at 0x803162: bytes 6 and 0, words 0x14 and 0xFFEA (-22). The
// eax it leaves (Window_Alloc's) is read by none of its 18 callers.
extern "C" void __cdecl Battle_OpenMsgWindow() {
    g.window_alloc(0, 3);
    unsigned char* const w = At(at::kMsgWindow);
    w[0] = 6;
    w[1] = 0;
    SetWord(w + 2, 0x14);
    SetWord(w + 4, 0xFFEA);
}

// original 0x445550 (PSX 0x801DB2C0, the same): 1 when the actor is present
// (+0 bit 0) with none of status 0x4944 (party +0x90) or 0x4144 (enemy
// +0x92). The whole eax as the original leaves it: the answer in al over the
// record's offset (actor * 0x14C, or (actor - 3) * 0x128 as a byte index),
// which Battle_MarkFasterSide's sums carry in their upper half.
extern "C" unsigned __cdecl Battle_ActorStanding(unsigned actor) {
    const unsigned a = actor & 0xFF;
    std::uint32_t offset;
    unsigned r;
    if (a < 3) {
        offset = a * at::kPartyStride;
        const unsigned char* const o = At(at::kParty + offset);
        r = (o[at::kPresent] & 1) && (Word(o + at::kPStatus) & 0x4944) == 0 ? 1 : 0;
    } else {
        offset = ((a - 3) & 0xFF) * at::kEnemyStride;
        const unsigned char* const o = At(at::kEnemy + offset);
        r = (o[at::kPresent] & 1) && (Word(o + at::kEStatus) & 0x4144) == 0 ? 1 : 0;
    }
    return (offset & ~0xFFu) | r;
}

// original 0x4455C0 (PSX 0x801DB368, the same): 1 when the member's agility
// (+0xA8) is at least twice the average's low word and at least the
// highest's low word. The whole eax: the agility's high byte over the answer.
extern "C" unsigned __cdecl Battle_PartyOutpaces(unsigned actor, unsigned average, unsigned highest) {
    const unsigned agility = Word(P(actor) + at::kPAgi);
    unsigned r = 1;
    if ((average & 0xFFFF) * 2 > agility) r = 0;
    else if ((highest & 0xFFFF) > agility) r = 0;
    return (agility & 0xFF00) | r;
}

// ===========================================================================
// The set-up and per-turn bookkeeping

// original 0x4301B0 (PSX 0x801D4820, the same but for reading the actor again
// after the first part - nothing between can change it): the acting actor
// (0x904B34's low byte) loses flag 0x40 (party +0x134, enemy +0x114) with its
// counter +0x144 / +0x124 zeroed, unless the command kind (the next byte) is 4
// with ability 0x27 (0x904B80); then flag 0x80 with +0x145 / +0x125, unless
// kind 4 with ability 0xA3. An actor above 2 is an enemy, (actor - 3) * 0x128
// - unbounded, as the original's.
extern "C" void __cdecl Battle_ClearActingFlags() {
    const std::uint32_t acting = static_cast<std::uint32_t>(Long(At(at::kActing)));
    const unsigned a = acting & 0xFF, kind = (acting >> 8) & 0xFF;
    const unsigned ability = Word(At(at::kAbility));
    const auto clear = [a](unsigned bit, unsigned party_count, unsigned enemy_count) {
        const bool party = a <= 2;
        unsigned char* const o = party ? P(a) : E(a);
        unsigned char* const flags = o + (party ? at::kPFlags : at::kEFlags);
        if ((flags[0] & bit) == 0) return;
        o[party ? party_count : enemy_count] = 0;
        SetLong(flags, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(flags)) & ~bit));
    };
    if (!(kind == 4 && ability == 0x27)) clear(0x40, at::kPCount40, at::kECount40);
    if (!(kind == 4 && ability == 0xA3)) clear(0x80, at::kPCount80, at::kECount80);
}

// original 0x4303D0 (PSX 0x801D4D14, the same): for each member the chain
// does not skip, +0x142 up by one while at most 5 with flag 1 (+0x134), +0x143
// likewise with status 0x800 (+0x91 bit 3), then +0x142 again while at most 2
// with flag 0x4000 (read after the first step); for each enemy, +0x122 while
// at most 2 with flag 0x4000. Then the timer 0x904B8E, if running, counts
// down, and when it reaches 0 every actor 0x4456C0 does not rule out loses
// flag 0x10.
extern "C" void __cdecl Battle_TickCounters() {
    for (unsigned i = 0; i <= 2; ++i) {
        if (Yes(g.skipped(i))) continue;
        unsigned char* const o = P(i);
        if ((o[at::kPFlags] & 1) && o[at::kPCount4000] <= 5) ++o[at::kPCount4000];
        if ((o[at::kPStatus + 1] & 8) && o[at::kPCount800] <= 5) ++o[at::kPCount800];
        if ((Long(o + at::kPFlags) & 0x4000) && o[at::kPCount4000] <= 2) ++o[at::kPCount4000];
    }
    for (unsigned i = 3; i <= 10; ++i) {
        if (Yes(g.skipped(i))) continue;
        unsigned char* const o = E(i);
        if ((Long(o + at::kEFlags) & 0x4000) && o[at::kECount4000] <= 2) ++o[at::kECount4000];
    }
    unsigned char* const timer = At(at::kTimer);
    if (*timer == 0) return;
    if (--*timer != 0) return;
    for (unsigned i = 0; i <= 2; ++i)
        if (!Yes(g.actor_absent(i))) P(i)[at::kPFlags] &= 0xEF;
    for (unsigned i = 3; i <= 10; ++i)
        if (!Yes(g.actor_absent(i))) E(i)[at::kEFlags] &= 0xEF;
}

// original 0x4453C0 (PSX 0x801DB020, the same): the enemies standing
// (Battle_ActorStanding) give an average and a highest agility (+0xB8); every
// member 0x445980 lets act and Battle_PartyOutpaces passes gets flag 0x8000
// (+0x134), and the answer is 1 when one did. Then the same the other way:
// the members' agility (+0xA8), and each enemy 0x445980 lets act and 0x445600
// passes gets flag 0x8000 (+0x114).
// As the original has it: the sums and the highest are built from the whole
// eax Battle_ActorStanding leaves (its upper half over the agility word), the
// sum's low word divided by the count only when it is not 0 - otherwise the
// whole sum is what the tests are given - and the highest replaced when its
// low word is below. Both tests read the low words alone.
extern "C" unsigned __cdecl Battle_MarkFasterSide() {
    const auto measure = [](unsigned from, unsigned to, bool party, std::uint32_t& average, std::uint32_t& highest) {
        std::uint32_t sum = 0, top = 0;
        unsigned count = 0;
        for (unsigned i = from; i <= to; ++i) {
            const unsigned r = g.standing(i);
            if (!Yes(r)) continue;
            const std::uint32_t v = (r & 0xFFFF0000u) | Word((party ? P(i) + at::kPAgi : E(i) + at::kEAgi));
            sum += v;
            if ((top & 0xFFFF) < (v & 0xFFFF)) top = v;
            count = (count + 1) & 0xFF;
        }
        if (sum & 0xFFFF) sum = static_cast<std::uint32_t>(static_cast<int>(sum & 0xFFFF) / static_cast<int>(count));
        average = sum;
        highest = top;
    };
    std::uint32_t average, highest;
    measure(3, 10, false, average, highest);
    unsigned marked = 0;
    for (unsigned i = 0; i <= 2; ++i) {
        if (!Yes(g.can_act(i))) continue;
        if (!Yes(g.outpaces(i, average, highest))) continue;
        unsigned char* const flags = P(i) + at::kPFlags;
        SetLong(flags, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(flags)) | 0x8000u));
        marked = (marked + 1) & 0xFF;
    }
    const unsigned answer = marked != 0 ? 1 : 0;
    measure(0, 2, true, average, highest);
    for (unsigned i = 3; i <= 10; ++i) {
        if (!Yes(g.can_act(i))) continue;
        if (!Yes(g.enemy_outpaces(i, average, highest))) continue;
        unsigned char* const flags = E(i) + at::kEFlags;
        SetLong(flags, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(flags)) | 0x8000u));
    }
    return answer;
}

// original 0x44F4B0 (PSX 0x800A0680, the same): clears the actor's status
// bits (party +0x90, enemy +0x92) that the mask names - 0x4000, 0x800, 0x80,
// 0x40, 0x10, 8, 4, 2, 1 each clear their own bit - and, when the mask has
// 0x20 and the status has it, bit 0x20 too with the 12 bytes at +0x34 put
// back from 0x93A034 + actor * 0x84. Then Sprite_ReleaseTint(the actor
// object), the status stored, and 0x446BB0(status) with Sprite_Current the
// actor object for the call. Answers the status word (the original's ax; its
// upper half is 0x446BB0's leftovers, which none of the 23 callers reads -
// most are effect handlers whose answer Effect_ApplyResult drops).
extern "C" unsigned __cdecl Battle_ClearStatus(unsigned actor, unsigned mask) {
    const unsigned a = actor & 0xFF;
    const bool party = a <= 2;
    unsigned char* const o = party ? P(a) : E(a);
    unsigned char* const status = o + (party ? at::kPStatus : at::kEStatus);
    unsigned s = Word(status) & ~(mask & 0x48DFu);
    if ((mask & 0x20) && (s & 0x20)) {
        s &= ~0x20u;
        std::memcpy(o + at::kPos, At(at::kSavedPos + a * at::kSavedPosStride), 12);
    }
    g.release_tint(o);
    SetWord(status, s);
    unsigned char* const saved = Sprite_Current;
    Sprite_Current = o;
    g.status_tint(s);
    Sprite_Current = saved;
    return s & 0xFFFF;
}

// original 0x446EA0 (PSX 0x801DDFB4, the same): for a member (above 2,
// nothing) found among the queue bytes 0x904ACC + (0x904AE2 - 1) up to
// 0x904AE3 (bytes, the first index 0xFF when 0x904AE2 is 0 - then nothing),
// whose queued action is an item (+0x125 = 5) with a category byte of 0
// (+0x127): 0x446D90(slot +0x12E, item word +0x126) gives it back. Not kept:
// the original leaves the loop index in its own argument slot, and eax as
// it falls; its callers pop the slot and put al over eax (the one callee they
// pass it to, 0x446650, reads its low byte - checked at all 12 call sites).
extern "C" void __cdecl Battle_ReturnQueuedItem(unsigned actor) {
    const unsigned a = actor & 0xFF;
    if (a > 2) return;
    const unsigned to = At(at::kQueueTo)[0];
    unsigned i = static_cast<unsigned char>(At(at::kQueueFrom)[0] - 1);
    for (;;) {
        if (i >= to) return;
        if (At(at::kItemQueue + i)[0] == a) break;
        ++i;
    }
    unsigned char* const o = P(a);
    if (o[at::kPQueuedKind] != 5) return;
    if (o[at::kPQueuedItem + 1] != 0) return;
    g.return_item(o[at::kPItemSlot], Word(o + at::kPQueuedItem));
}

// original 0x44AA00 (PSX 0x801DEA9C, the same): one of two system lines at
// random (Rand & 1) by 0x904B90: 1 lines 0 / 1, 2..4 lines 2 / 3, 0 or above
// 4 lines 4 / 0 - the six bytes the original builds on its stack - put
// through 0x44A6E0(0, 2, 0, 0, 0xFF, text).
extern "C" void __cdecl Battle_OpeningMessage() {
    static constexpr unsigned char kLines[6] = {0, 1, 2, 3, 4, 0};
    const std::uint32_t kind = static_cast<std::uint32_t>(Long(At(at::kOpening)));
    const unsigned base = kind == 1 ? 0 : (kind == 0 || kind > 4) ? 4 : 2;
    const unsigned id = kLines[base + (static_cast<unsigned>(g.rand()) & 1)];
    g.message_at(0, 2, 0, 0, 0xFF, g.msg(id));
}

// ===========================================================================
// The chain's nine steps (0x430510 calls them in this order)

// original 0x430640 (PSX 0x801D524C, the same): each actor the chain does not
// skip with flag 0x4000 and its counter at 3 loses the flag, the counter 0,
// the HP change word takes HP (party +0x128 = +0x98, enemy +0x108 = +0xA4),
// the effect is marked (+0x12C / +0x10C = 0x11) and 0x446FB0 told; if any,
// the window and line 0x2D. Answers 1 when it showed a line.
// As the original has it (D-number in docs/battle_setup.md section 6): an
// enemy's counter is read from the PARTY array at the enemy's actor index -
// 0x802D40 + actor * 0x14C + 0x142, past the three members - while the one
// it zeroes is the enemy's own +0x122. The PSX reads 0x80145E8C + actor *
// 0x140 + 0x136 the same way.
extern "C" unsigned __cdecl BattleStep_Expire4000() {
    unsigned count = 0;
    for (unsigned i = 0; i <= 2; ++i) {
        if (Yes(g.skipped(i))) continue;
        unsigned char* const o = P(i);
        unsigned char* const flags = o + at::kPFlags;
        if ((Long(flags) & 0x4000) == 0 || o[at::kPCount4000] != 3) continue;
        o[at::kPCount4000] = 0;
        SetLong(flags, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(flags)) & ~0x4000u));
        SetWord(o + at::kPHpChange, Word(o + at::kPHp));
        MarkEffect(o);
        o[at::kPEffect] = 0x11;
        count = (count + 1) & 0xFF;
        g.set_pending(i);
    }
    for (unsigned i = 3; i <= 10; ++i) {
        if (Yes(g.skipped(i))) continue;
        unsigned char* const o = E(i);
        unsigned char* const flags = o + at::kEFlags;
        if ((Long(flags) & 0x4000) == 0 || P(i)[at::kPCount4000] != 3) continue;   // the party array, as the original
        o[at::kECount4000] = 0;
        SetLong(flags, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(flags)) & ~0x4000u));
        SetWord(o + at::kEHpChange, Word(o + at::kEHp));
        MarkEffect(o);
        o[at::kEEffect] = 0x11;
        count = (count + 1) & 0xFF;
        g.set_pending(i);
    }
    if (count == 0) return 0;
    g.open_window();
    ShowLine(g.msg(0x2D));
    return 1;
}

// original 0x430790 (PSX 0x801D54C8, the same): each member the chain does not
// skip with status 0x800 and its counter +0x143 at 5: the counter 0, HP =
// max HP, AP = max AP, +0x9C = +0xAE, then Battle_ClearStatus(member, 0x8FF).
// If any, the window, and with one member its name (0x44A910) and line 0x2B,
// with more line 0x2C. Enemies are not looked at.
extern "C" unsigned __cdecl BattleStep_Restore800() {
    unsigned count = 0, who = 0;
    for (unsigned i = 0; i <= 2; ++i) {
        if (Yes(g.skipped(i))) continue;
        unsigned char* const o = P(i);
        if ((o[at::kPStatus + 1] & 8) == 0 || o[at::kPCount800] != 5) continue;
        count = (count + 1) & 0xFF;
        o[at::kPCount800] = 0;
        SetWord(o + at::kPHp, Word(o + at::kPMaxHp));
        SetWord(o + at::kPAp, Word(o + at::kPMaxAp));
        o[at::kPByte9C] = o[at::kPByteAE];
        who = i;
        g.clear_status(i, 0x8FF);
    }
    if (count == 0) return 0;
    g.open_window();
    const unsigned char* text;
    if (count == 1) {
        g.party_name(who);
        text = g.msg(0x2B);
    } else {
        text = g.msg(0x2C);
    }
    ShowLine(text);
    return 1;
}

namespace {

// The four wake steps, one template in the original's code: each actor of
// the side the chain does not skip whose status low byte has `bit` rolls
// 0x446CB0; on a hit its counter (party +0x12D, enemy +0x10D) is zeroed and
// the bit cleared, else the counter goes up by one (after the roll, which
// reads it). If any woke, the window, and with one its name and line
// `single`, with more line `many`.
unsigned WakeStep(bool party, unsigned bit, unsigned single, unsigned many) {
    unsigned count = 0, who = 0;
    const unsigned from = party ? 0 : 3, to = party ? 2 : 10;
    for (unsigned i = from; i <= to; ++i) {
        if (Yes(g.skipped(i))) continue;
        unsigned char* const o = party ? P(i) : E(i);
        unsigned char* const status = o + (party ? at::kPStatus : at::kEStatus);
        unsigned char* const counter = o + (party ? at::kPCounter : at::kECounter);
        if ((status[0] & bit) == 0) continue;
        if (Yes(g.wake_roll(i))) {
            *counter = 0;
            SetWord(status, Word(status) & ~bit);
            who = i;
            count = (count + 1) & 0xFF;
        } else {
            ++*counter;
        }
    }
    if (count == 0) return 0;
    g.open_window();
    const unsigned char* text;
    if (count == 1) {
        (party ? g.party_name : g.enemy_name)(who);
        text = g.msg(single);
    } else {
        text = g.msg(many);
    }
    ShowLine(text);
    return 1;
}

}  // namespace

// original 0x430890 (PSX 0x801D5628, the same): the members' status 0x40,
// lines 0x25 / 0x26.
extern "C" unsigned __cdecl BattleStep_PartyWake40() { return WakeStep(true, 0x40, 0x25, 0x26); }

// original 0x430970 (PSX 0x801D577C, the same): the enemies' status 0x40,
// lines 0x25 / 0x27.
extern "C" unsigned __cdecl BattleStep_EnemyWake40() { return WakeStep(false, 0x40, 0x25, 0x27); }

// original 0x430A50 (PSX 0x801D58DC, the same): the members' status 0x20,
// lines 0x28 / 0x29.
extern "C" unsigned __cdecl BattleStep_PartyWake20() { return WakeStep(true, 0x20, 0x28, 0x29); }

// original 0x430B30 (PSX 0x801D5A30, the same): the enemies' status 0x20,
// lines 0x28 / 0x2A.
extern "C" unsigned __cdecl BattleStep_EnemyWake20() { return WakeStep(false, 0x20, 0x28, 0x2A); }

// original 0x430C10 (PSX 0x801D5B90, the same): each actor the chain does not
// skip with status 0x80: both change words zeroed (party +0x12A then +0x128,
// enemy +0x10A / +0x108), 0x446540(actor) sets the HP change, the effect
// marked (+0x12C / +0x10C = 0x11) and 0x446FB0 told; if any, the window and
// line 0x18.
extern "C" unsigned __cdecl BattleStep_Status80() {
    unsigned count = 0;
    for (unsigned i = 0; i <= 10; ++i) {
        if (Yes(g.skipped(i))) continue;
        const bool party = i <= 2;
        unsigned char* const o = party ? P(i) : E(i);
        if ((o[party ? at::kPStatus : at::kEStatus] & 0x80) == 0) continue;
        SetWord(o + (party ? at::kPApChange : at::kEApChange), 0);
        SetWord(o + (party ? at::kPHpChange : at::kEHpChange), 0);
        g.set_hp_change(i);
        MarkEffect(o);
        count = (count + 1) & 0xFF;
        o[party ? at::kPEffect : at::kEEffect] = 0x11;
        g.set_pending(i);
    }
    if (count == 0) return 0;
    g.open_window();
    ShowLine(g.msg(0x18));
    return 1;
}

// original 0x430D40 (PSX 0x801D5D9C, the same): each member the chain does not
// skip gets an HP change word (+0x128, and +0x12A zeroed), a u16 built from
// 0: less 1 when 0x904060 is 5; less (max HP + 10) / 20 for character id 6
// (+0x89); again for id 7 or 0 with flag 2 (+0x134) while 0x904B89 is 0x12;
// with weapon 0x52 (+0x92) plus (max HP + 10) / 20, then cut to HP - 1 when
// it reaches HP (signed against the HP word); less one for armour byte 3
// (+0x95) 0x1F, for each accessory (+0x96, +0x97) 0x16, and each 0x17; less
// max HP / 2 with status 1 (+0x90). Not 0: the effect marked (+0x12C = 0x11)
// and 0x446FB0 told. Each enemy with status 1 (+0x92): HP change (+0x108)
// minus max HP (+0xB0) / 2, marked (+0x10C = 0x11), 0x446FB0. Answers whether
// any actor's bit is set in 0x904B82.
// The divisions are the original's: (max HP + 10) / -20 through the
// 0x99999999 multiply, which truncates as C does. The word is built in a
// local and stored once - nothing it passes reads it back.
extern "C" unsigned __cdecl BattleStep_HpDrift() {
    for (unsigned i = 0; i <= 2; ++i) {
        if (Yes(g.skipped(i))) continue;
        unsigned char* const o = P(i);
        std::uint16_t w = 0;
        SetWord(o + at::kPApChange, 0);
        if (At(at::kHpDriftMode)[0] == 5) --w;
        const int n = static_cast<int>(Word(o + at::kPMaxHp)) + 10;
        if (o[at::kPCharId] == 6) w = static_cast<std::uint16_t>(w + n / -20);
        const unsigned char id = o[at::kPCharId];
        if ((id == 7 || id == 0) && (o[at::kPFlags] & 2) && At(at::kField12)[0] == 0x12)
            w = static_cast<std::uint16_t>(w + n / -20);
        if (o[at::kPWeapon] == 0x52) {
            w = static_cast<std::uint16_t>(w + n / 20);
            const unsigned hp = Word(o + at::kPHp);
            if (static_cast<int>(static_cast<std::int16_t>(w)) >= static_cast<int>(hp)) w = static_cast<std::uint16_t>(hp - 1);
        }
        if (o[at::kPArmour3] == 0x1F) --w;
        if (o[at::kPAccessory] == 0x16) --w;
        if (o[at::kPAccessory + 1] == 0x16) --w;
        if (o[at::kPAccessory] == 0x17) --w;
        if (o[at::kPAccessory + 1] == 0x17) --w;
        if (o[at::kPStatus] & 1) w = static_cast<std::uint16_t>(w - (Word(o + at::kPMaxHp) >> 1));
        SetWord(o + at::kPHpChange, w);
        if (w == 0) continue;
        MarkEffect(o);
        o[at::kPEffect] = 0x11;
        g.set_pending(i);
    }
    for (unsigned i = 3; i <= 10; ++i) {
        if (Yes(g.skipped(i))) continue;
        unsigned char* const o = E(i);
        if ((o[at::kEStatus] & 1) == 0) continue;
        SetWord(o + at::kEHpChange, 0u - (Word(o + at::kEMaxHp) >> 1));
        MarkEffect(o);
        o[at::kEEffect] = 0x11;
        g.set_pending(i);
    }
    return Word(At(at::kPending)) != 0 ? 1 : 0;
}

// original 0x430F40 (PSX 0x801D62A8, the same): each member the chain does not
// skip with flag 2 and not flag 0x20 (+0x134) pays the AP cost, 0x904B78's
// low byte + 1 halved: with less AP (+0x9A) than that, +1 +2 +3 = 6, 4, 4 and
// 0x446FB0 told; otherwise both change words zeroed, the AP lowered, the AP
// change (+0x12A) the cost, and a cost not 0 marks the effect (+0x12C = 2)
// and tells 0x446FB0. A member without flag 2, or with 0x20, loses 0x20.
// Answers whether any actor's bit is set in 0x904B82.
extern "C" unsigned __cdecl BattleStep_ApUpkeep() {
    for (unsigned i = 0; i <= 2; ++i) {
        if (Yes(g.skipped(i))) continue;
        unsigned char* const o = P(i);
        const std::uint32_t flags = static_cast<std::uint32_t>(Long(o + at::kPFlags));
        if ((flags & 2) == 0 || (flags & 0x20) != 0) {
            SetLong(o + at::kPFlags, static_cast<std::int32_t>(flags & ~0x20u));
            continue;
        }
        const int cost = static_cast<int>((static_cast<std::uint32_t>(Long(At(at::kApCost))) & 0xFF) + 1) / 2;
        if (static_cast<int>(Word(o + at::kPAp)) < cost) {
            o[at::kState1] = 6;
            o[at::kState2] = 4;
            o[at::kState3] = 4;
            g.set_pending(i);
            continue;
        }
        SetWord(o + at::kPHpChange, 0);
        SetWord(o + at::kPApChange, 0);
        SetWord(o + at::kPAp, Word(o + at::kPAp) - static_cast<unsigned>(cost));
        SetWord(o + at::kPApChange, static_cast<unsigned>(cost));
        if ((cost & 0xFFFF) == 0) continue;
        MarkEffect(o);
        o[at::kPEffect] = 2;
        g.set_pending(i);
    }
    return Word(At(at::kPending)) != 0 ? 1 : 0;
}

// ===========================================================================

void BattleSetup_Inject() {
    if (bof3::WantsShadow("battle_setup")) battle_setup::SelfTest();
    BOF3_INJECT(Battle_ClearActingFlags);
    BOF3_INJECT(Battle_TickCounters);
    BOF3_INJECT(BattleStep_Expire4000);
    BOF3_INJECT(BattleStep_Restore800);
    BOF3_INJECT(BattleStep_PartyWake40);
    BOF3_INJECT(BattleStep_EnemyWake40);
    BOF3_INJECT(BattleStep_PartyWake20);
    BOF3_INJECT(BattleStep_EnemyWake20);
    BOF3_INJECT(BattleStep_Status80);
    BOF3_INJECT(BattleStep_HpDrift);
    BOF3_INJECT(BattleStep_ApUpkeep);
    BOF3_INJECT(Battle_ActorSkipped);
    BOF3_INJECT(Battle_MarkFasterSide);
    BOF3_INJECT(Battle_ActorStanding);
    BOF3_INJECT(Battle_PartyOutpaces);
    BOF3_INJECT(Battle_OpenMsgWindow);
    BOF3_INJECT(Battle_ReturnQueuedItem);
    BOF3_INJECT(Battle_OpeningMessage);
    BOF3_INJECT(Battle_ClearStatus);
}

// The enemy AI script ops: what an enemy object does in its state 0, which
// BattleEnemy_RunAll (battle_flow.cpp) enters through the state table
// 0x64B084. The state's step byte +1 picks an op from EnemyOp_Steps
// 0x64B1A0; an op's own sub-steps +2 and +3 pick from the six tables after
// it. Twenty-two of those ops are here: the step dispatch, the entrance (a
// scale-in), the idle loop with its target highlight and bob, the hit the
// enemy takes and its end, and the death animation that ends in
// Battle_EnemyDefeated. docs/enemy_ai_ops.md.
//
// Every call goes through enemy_ai_ops::g (enemy_ai_ops_callees.h), so that
// the start-up fuzz can stand recorders in for them - for ours and for the
// originals' copies alike. Everything here is a faithful replacement.
//
// Sprite_Current (0x937F88) and 0x939AD8 are both the enemy being run while
// BattleEnemy_RunAll dispatches; the originals read each afresh at most
// stores and after every call, and so does this file: a store's object is
// whichever pointer the original loads for it, at the point it loads it.
#include "game/enemy_ai_ops.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/enemy_ai_ops_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace enemy_ai_ops {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    BattleEnemy_SetAnimation, BattleEnemy_ScriptTick, BattleEnemy_ScriptTickOnce, BattleEnemy_Chance70,
    Battle_EnemyDefeated,
    Sprite_ScriptTickOnce, Battle_StatusTint, Sprite_SetTint, Tint_Release, Sprite_ReleaseTint, Battle_ApplyDamage,
    Effect_ApplyResult, Fn<unsigned (__cdecl*)(unsigned, unsigned)>(kItemClass), Battle_SetDamagePopup,
    Fn<void (__cdecl*)(unsigned, unsigned)>(kApPopup), EnemyAI_TurnCheck, Fn<void (__cdecl*)(unsigned)>(kPlayCue),
    Sound_PlayEffect, Battle_PlayHitSound, Battle_SetHitPopup, Battle_ClearActorBit,
};
Callees g = kOriginals;

}  // namespace enemy_ai_ops

using namespace enemy_ai_ops;

namespace {

// A pointer the original keeps in a dword, read afresh.
unsigned char* PtrAt(std::uint32_t address) {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(At(address)))));
}
unsigned char* PtrIn(const unsigned char* p) {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(p))));
}
void SetPtrAt(std::uint32_t address, const unsigned char* p) {
    SetLong(At(address), static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(p)));
}
std::uint32_t ULong(const unsigned char* p) { return static_cast<std::uint32_t>(Long(p)); }
void SetULong(unsigned char* p, std::uint32_t v) { SetLong(p, static_cast<std::int32_t>(v)); }
short SWord(const unsigned char* p) { return static_cast<short>(Word(p)); }

// 0x939AD8 and Sprite_Current, each re-read wherever the original re-reads it.
unsigned char* Enemy() { return PtrAt(at::kEnemyCurrent); }
unsigned char* Sc() { return Sprite_Current; }

// The op tables: entry `index` of the code pointers at `table`. As the
// originals have it, the index is not checked - it is one of the object's
// step bytes, and a step past a table's end runs the next table's entries.
Handler Entry(std::uint32_t table, unsigned index) {
    return reinterpret_cast<const Handler*>(static_cast<std::uintptr_t>(table))[index];
}

// The 24-byte ability record's flag bytes, by the word 0x904B80 read afresh.
unsigned char AbilityByte(std::uint32_t flags) { return At(flags + Word(At(at::kMagicId)) * 24u)[0]; }

// The enemy's +0xF4 hook.
using HookFn = void (__cdecl*)(int);

}  // namespace

// ===========================================================================
// The dispatchers
// ===========================================================================

// original 0x4360F0 (PSX 0x801E3140): state 0 of the enemy state table
// 0x64B084 - a tail jump through EnemyOp_Steps 0x64B1A0 by Sprite_Current +1.
extern "C" void __cdecl EnemyOp_StepDispatch(void) { Entry(at::kSteps, Sc()[1])(); }

// original 0x436170 (PSX 0x801E3224): step 1, the entrance - through
// EnemyOp_EnterSubs 0x64B1D4 by +2 (0x436190, 0x436270).
extern "C" void __cdecl EnemyOp_EnterDispatch(void) { Entry(at::kEnterSubs, Sc()[2])(); }

// original 0x436190 (PSX 0x801E3268): the entrance's first part, the
// scale-in - through EnemyOp_ScaleInSubs 0x64B1DC by +3 (0x4361B0, 0x436210).
extern "C" void __cdecl EnemyOp_ScaleInDispatch(void) { Entry(at::kScaleInSubs, Sc()[3])(); }

// original 0x4366E0 (PSX 0x801E3AE0): step 6 - through EnemyOp_ActSubs
// 0x64B1FC by +2 (0x436700, 0x436720, 0x436700, 0x436BC0, 0x436D90, 0x436F00).
extern "C" void __cdecl EnemyOp_ActDispatch(void) { Entry(at::kActSubs, Sc()[2])(); }

// original 0x436720 (PSX 0x801E3B48): the hit taken - through
// EnemyOp_HitSubs 0x64B214 by +3 (0x436740, 0x436A00, 0x436A20).
extern "C" void __cdecl EnemyOp_HitDispatch(void) { Entry(at::kHitSubs, Sc()[3])(); }

// original 0x436D90 (PSX 0x801E46B8): the death - through EnemyOp_DeathSubs
// 0x64B234 by +3 (0x436DB0, 0x436E00, 0x436E40, 0x436EC0).
extern "C" void __cdecl EnemyOp_DeathDispatch(void) { Entry(at::kDeathSubs, Sc()[3])(); }

// ===========================================================================
// The entrance and the idle loop
// ===========================================================================

// original 0x436110 (PSX 0x801E3184): step 0. The enemy's animation table
// +0xFC = 0x64B078 (the bytes 0..7); the dwords 0x904B64 / 0x904B68 =
// 0x437720 / 0x437750; animation 0; Sprite_Current +0 |= 0x40;
// Battle_StatusTint(the enemy's status word +0x92); step +1 on.
//
// As the original has it: 0x939AD8 is read afresh for the status word, and
// Sprite_Current after each call. The status goes out as a word with the
// upper half of whatever edx held; Battle_StatusTint reads bit 7 alone.
extern "C" void __cdecl EnemyOp_Begin(void) {
    SetULong(Enemy() + 0xFC, at::kAnimTable);
    SetULong(At(at::kHookA), at::kHookAValue);
    SetULong(At(at::kHookB), at::kHookBValue);
    g.set_animation(0);
    Sc()[0] = static_cast<unsigned char>(Sc()[0] | 0x40);
    g.status_tint(Word(Enemy() + 0x92));
    Sc()[1] = static_cast<unsigned char>(Sc()[1] + 1);
}

// original 0x4361B0 (PSX 0x801E32AC): the scale-in's start. Sprite_Current
// +0 &= ~0x40, then animation 0; +0x48 = 2, the dwords +0x44 / +0x40 / +0xC
// = 0 and +0x18 = 0x666; +3 = 1.
extern "C" void __cdecl EnemyOp_ScaleInStart(void) {
    Sc()[0] = static_cast<unsigned char>(Sc()[0] & 0xBF);
    g.set_animation(0);
    Sc()[0x48] = 2;
    SetULong(Sc() + 0x44, 0);
    SetULong(Sc() + 0x40, 0);
    SetULong(Sc() + 0xC, 0);
    SetULong(Sc() + 0x18, 0x666);
    Sc()[3] = 1;
}

// original 0x436210 (PSX 0x801E3310): the scale-in, a frame. While the s32
// +0x40 is below 0x10000 (1.0): +0x40 = it + the speed +0xC, +0x44 += +0xC,
// the speed += +0x18. Otherwise +0x48 = 0 and the steps +1..+3 = 3, 0, 0.
// Either way a tail jump to BattleEnemy_ScriptTick.
extern "C" void __cdecl EnemyOp_ScaleInStep(void) {
    unsigned char* const s = Sc();
    const std::uint32_t scale = ULong(s + 0x40);
    if (static_cast<std::int32_t>(scale) < 0x10000) {
        SetULong(s + 0x40, ULong(s + 0xC) + scale);
        SetULong(Sc() + 0x44, ULong(Sc() + 0x44) + ULong(Sc() + 0xC));
        SetULong(Sc() + 0xC, ULong(Sc() + 0xC) + ULong(Sc() + 0x18));
    } else {
        s[0x48] = 0;
        Sc()[1] = 3;
        Sc()[2] = 0;
        Sc()[3] = 0;
    }
    g.script_tick();
}

// original 0x4363B0 (PSX 0x801E3550): step 2. Animation 8 when bit 1 of the
// enemy's +0x110 is set, else 0; BattleEnemy_ScriptTick (its answer
// unused); step +1 on.
extern "C" void __cdecl EnemyOp_Idle(void) {
    g.set_animation((Enemy()[0x110] & 2) != 0 ? 8u : 0u);
    g.script_tick();
    Sc()[1] = static_cast<unsigned char>(Sc()[1] + 1);
}

// original 0x4363E0 (PSX 0x801E35B0): step 3, the idle loop, every frame
// until something else moves the step. First the highlight sub-op of
// EnemyOp_WaitSubs 0x64B1EC by +2 (called). Then, while the enemy's status
// +0x92 has 0x20, a bob: the byte +9 of the battle-task slot numbered by
// the actor byte +5 steps up while bit 11 of +0x110 is set and down while
// it is clear, and the bit flips when the byte reaches 12 or -12; the
// sprite's +0x34 (modes 1 and 3) or +0x38 (the others) = the slot's same
// dword + (the s8 byte << 9). Last, unless bit 12 of +0x110 is set, a tail
// jump to BattleEnemy_ScriptTick.
//
// As the original has it: 0x939AD8 and Sprite_Current are read once after
// the call; the actor byte and the slot's byte are re-read at each use (a
// slot past the 48 is not refused - the actor byte is 3..10 for an enemy).
extern "C" void __cdecl EnemyOp_Wait(void) {
    Entry(at::kWaitSubs, Sc()[2])();
    unsigned char* const e = Enemy();
    if ((e[0x92] & 0x20) != 0) {
        unsigned char* const s = Sc();
        const auto slot = [s] { return At(at::kTasks + s[5] * at::kTaskSize); };
        bool flip;
        if ((ULong(e + 0x110) & 0x800) != 0) {
            slot()[9] = static_cast<unsigned char>(slot()[9] + 1);
            flip = static_cast<signed char>(slot()[9]) >= 0xC;
        } else {
            slot()[9] = static_cast<unsigned char>(slot()[9] - 1);
            flip = static_cast<signed char>(slot()[9]) <= -0xC;
        }
        if (flip) SetULong(e + 0x110, ULong(e + 0x110) ^ 0x800u);
        const unsigned mode = s[8];
        const unsigned field = (mode == 1 || mode == 3) ? 0x34u : 0x38u;
        unsigned char* const t = slot();
        const std::int32_t bob = static_cast<signed char>(t[9]);
        SetULong(s + field, static_cast<std::uint32_t>(bob << 9) + ULong(t + field));
    }
    if ((ULong(Enemy() + 0x110) & 0x1000) == 0) g.script_tick();
}

// original 0x436510 (PSX 0x801E37B4): EnemyOp_Wait's sub-op 0. While a
// target is being picked (0x904AAF) and the command's target byte (the byte
// at the pointer 0x939FA0) is this actor (+5) or has 0x40 (a whole side), a
// tint record: Sprite_SetTint(Sprite_Current, 0, 0, 0, 0), its index to +7;
// +2 = 1.
extern "C" void __cdecl EnemyOp_HighlightOn(void) {
    if (At(at::kTargeting)[0] == 0) return;
    const unsigned char target = PtrAt(at::kCommandSource)[0];
    unsigned char* const s = Sc();
    if (target != s[5] && (target & 0x40) == 0) return;
    const unsigned char tint = g.set_tint(s, 0, 0, 0, 0);
    Sc()[7] = tint;
    Sc()[2] = 1;
}

// original 0x436560 (PSX 0x801E3838): sub-op 1, the highlight's pulse. The
// tint record +7's r g b = the menu cursor's pulse 0x904AC8. When no target
// is being picked any more, or the target byte is neither this actor nor a
// side, Tint_Release(+7) and +2 = 0.
//
// As the original has it: Sprite_Current is read once before the call; +7
// is read again for each store; an index of 0xFF (Sprite_SetTint found no
// free record) writes past the 32 records.
extern "C" void __cdecl EnemyOp_HighlightPulse(void) {
    unsigned char* const s = Sc();
    const unsigned char pulse = At(at::kPulse)[0];
    At(at::kTints + s[7] * at::kTintSize + 4)[0] = pulse;
    At(at::kTints + s[7] * at::kTintSize + 3)[0] = pulse;
    At(at::kTints + s[7] * at::kTintSize + 2)[0] = pulse;
    if (At(at::kTargeting)[0] != 0) {
        const unsigned char target = PtrAt(at::kCommandSource)[0];
        if (target == s[5]) return;
        if ((target & 0x40) != 0) return;
    }
    g.tint_release(s[7]);
    Sc()[2] = 0;
}

// ===========================================================================
// The hit taken
// ===========================================================================

// original 0x436700 (PSX 0x801E3B24): EnemyOp_ActSubs' entries 0 and 2 -
// Sprite_Current +4 = 0, +2 = 1.
extern "C" void __cdecl EnemyOp_ActBegin(void) {
    Sc()[4] = 0;
    Sc()[2] = 1;
}

// original 0x436740 (PSX Battle_ResolveAction_Enemy 0x801E3B8C, the
// sibling's hypothesis name): the action aimed at this enemy lands.
//  - The status words 0x904B98 / 0x904B9A = 0; the result's sprite 0x904B5C
//    = Sprite_Current, its target 0x904B54 = the actor +5, the record
//    0x904B60 = the enemy +0x104, whose HP delta +4, AP delta +6 and flags
//    +8 are zeroed; the enemy's 32 bytes +0xB0.. copied to 0x939F80.
//  - The effect, by the action kind 0x904B35: 1 (an attack) - the HP delta
//    = Battle_ApplyDamage(0x904B34, 0x904B44); 4 and 5 (an ability, an item)
//    - Effect_ApplyResult; 0, 2, 3 and above 5 - nothing.
//  - In an event battle, the enemy's +0xF4 hook with 1.
//  - Flags bit 0 with an HP delta of 0: +2 = 3 (a miss) and nothing else.
//  - Unless bit 9 of +0x110: animation 4 (the hit) for an attack, for an
//    item whose class (0x591810(0x904B81, 0x904B80)) lacks 4, and for an
//    ability whose record's byte +8 lacks 4.
//  - Flags bit 0: the damage pop-up (the HP delta, the actor); bit 1: the AP
//    pop-up (0x453EB0).
//  - The status +0x92 |= 0x904B98, which is zeroed; EnemyAI_TurnCheck.
//  - An HP delta above 0 with the HP +0xA4 at 0: +9 = 0, with round flag
//    0x80 a tint (0x10, 0x10, 0x10), and the status = 0x4000 (dead) - not
//    ORed. Above 0 with HP left in an event battle: the cue 0x437450(the
//    word +4 at the enemy's +0xF8).
//  - Either delta below 0 (a heal): Sound_PlayEffect(0x206).
//  - Unless round flag 0x2000: the hit sound and the hit pop-up for an
//    attack, and for an ability whose byte +0xD has 8.
//  - Round flag 0x80 cleared; BattleEnemy_ScriptTick; +3 on.
//
// As the original has it: 0x939AD8 is re-read after every call, and so is
// Sprite_Current; 0x904B35 is read afresh at every test; the result record
// is written through 0x904B60 read afresh. The arguments go out as the
// original's bytes and words (its pushes carry other registers' upper
// bits, which every callee masks off).
extern "C" void __cdecl EnemyOp_ReceiveAction(void) {
    unsigned char* s = Sc();
    unsigned char* e = Enemy();
    SetWord(At(at::kStatusAdd2), 0);
    SetWord(At(at::kStatusAdd), 0);
    const unsigned char actor = s[5];
    SetPtrAt(at::kResultSprite, s);
    At(at::kResultTarget)[0] = actor;
    SetPtrAt(at::kResult, e + 0x104);
    SetWord(PtrAt(at::kResult) + 4, 0);
    SetWord(PtrAt(at::kResult) + 6, 0);
    PtrAt(at::kResult)[8] = 0;
    e = Enemy();
    for (unsigned i = 0; i < 32; i += 4) SetLong(At(at::kStatBlock + i), Long(e + 0xB0 + i));
    const unsigned char kind = At(at::kActionKind)[0];
    if (kind == 1) {
        const unsigned target = At(at::kTarget)[0];
        const unsigned attacker = At(at::kActor)[0];
        const short delta = g.apply_damage(attacker, target);
        SetWord(PtrAt(at::kResult) + 4, static_cast<std::uint16_t>(delta));
        e = Enemy();
    } else if (kind > 3 && kind <= 5) {
        g.effect_apply();
        e = Enemy();
    }
    if (At(at::kEventBattle)[0] != 0) {
        reinterpret_cast<HookFn>(PtrIn(e + 0xF4))(1);
        e = Enemy();
    }
    if ((e[0x10C] & 1) != 0 && Word(e + 0x108) == 0) {
        Sc()[2] = 3;
        return;
    }
    if ((ULong(e + 0x110) & 0x200) == 0) {
        if (At(at::kActionKind)[0] == 1) {
            g.set_animation(4);
            e = Enemy();
        }
        if (At(at::kActionKind)[0] == 5) {
            const unsigned id = At(at::kMagicId)[0];
            const unsigned category = At(at::kMagicId + 1)[0];
            if ((g.item_class(category, id) & 4) == 0) g.set_animation(4);
            e = Enemy();
        }
        if (At(at::kActionKind)[0] == 4 && (AbilityByte(at::kAbilityFlags8) & 4) == 0) {
            g.set_animation(4);
            e = Enemy();
        }
    }
    if ((e[0x10C] & 1) != 0) {
        g.damage_popup(Word(e + 0x108), Sc()[5]);
        e = Enemy();
    }
    if ((e[0x10C] & 2) != 0) {
        g.ap_popup(Word(e + 0x10A), Sc()[5]);
        e = Enemy();
    }
    SetWord(e + 0x92, Word(e + 0x92) | Word(At(at::kStatusAdd)));
    SetWord(At(at::kStatusAdd), 0);
    g.turn_check();
    e = Enemy();
    if (SWord(e + 0x108) > 0) {
        if (Word(e + 0xA4) == 0) {
            Sc()[9] = 0;
            if ((At(at::kRoundFlags)[0] & 0x80) != 0) g.set_tint(Sc(), 0x10, 0x10, 0x10, 0);
            SetWord(Enemy() + 0x92, 0x4000);
            e = Enemy();
        } else if (At(at::kEventBattle)[0] != 0) {
            g.play_cue(Word(PtrIn(e + 0xF8) + 4));
            e = Enemy();
        }
    }
    if (SWord(e + 0x108) < 0 || SWord(e + 0x10A) < 0) g.play_effect(0x206);
    if ((ULong(At(at::kRoundFlags)) & 0x2000) == 0) {
        if (At(at::kActionKind)[0] == 1) {
            g.hit_sound();
            g.hit_popup();
        }
        if (At(at::kActionKind)[0] == 4 && (AbilityByte(at::kAbilityFlagsD) & 8) != 0) {
            g.hit_sound();
            g.hit_popup();
        }
    }
    SetWord(At(at::kRoundFlags), Word(At(at::kRoundFlags)) & 0xFF7F);
    g.script_tick();
    Sc()[3] = static_cast<unsigned char>(Sc()[3] + 1);
}

// original 0x436A00 (PSX 0x801E406C): +3 on once BattleEnemy_ScriptTickOnce
// answers non-zero (the animation is done).
extern "C" void __cdecl EnemyOp_WaitAnimOnce(void) {
    if (g.script_tick_once() != 0) Sc()[3] = static_cast<unsigned char>(Sc()[3] + 1);
}

// original 0x436A20 (PSX 0x801E40B4): the hit's end. Sprite_ScriptTickOnce
// (the sprite's own, not BattleEnemy_ScriptTickOnce), then:
//  - a dead enemy (+0x93 bit 6): Sprite_ReleaseTint(Sprite_Current), round
//    flag 0x40 cleared, the steps +1..+3 = 6, 4, 0 (the death) - and nothing
//    else;
//  - round flag 0x40 already set: it becomes 0x1000;
//  - otherwise, unless the target 0x904B44 has 0x40 or 0x80 (a side), round flag 0x40
//    is set on BattleEnemy_Chance70 for an attack, and for an ability whose
//    record's +0xD has 8 and whose +8 lacks 0x10 (Chance70 before the
//    second test).
//  - Then Battle_ClearActorBit(+5), the enemy's +0x110 bits 4..6 and 9
//    cleared, the steps = 2, 0, 0 (back to the idle loop).
//
// As the original has it: the 0x40 -> 0x1000 store is of the round flags'
// word, from the dword read with the flag's test; 0x904B80 is re-read after
// Chance70; 0x939AD8 is re-read for each of the two +0x110 stores.
extern "C" void __cdecl EnemyOp_HitEnd(void) {
    g.sprite_tick_once();
    if ((Enemy()[0x93] & 0x40) != 0) {
        g.release_tint(Sc());
        unsigned char* const s = Sc();
        SetWord(At(at::kRoundFlags), Word(At(at::kRoundFlags)) & 0xFFBF);
        s[1] = 6;
        Sc()[2] = 4;
        Sc()[3] = 0;
        return;
    }
    if ((At(at::kRoundFlags)[0] & 0x40) != 0) {
        SetWord(At(at::kRoundFlags), (Word(At(at::kRoundFlags)) & 0xFFBF) | 0x1000);
    } else if ((At(at::kTarget)[0] & 0xC0) == 0) {
        if (At(at::kActionKind)[0] == 1 && g.chance70() != 0)
            At(at::kRoundFlags)[0] = static_cast<unsigned char>(At(at::kRoundFlags)[0] | 0x40);
        if (At(at::kActionKind)[0] == 4 && (AbilityByte(at::kAbilityFlagsD) & 8) != 0 && g.chance70() != 0 &&
            (AbilityByte(at::kAbilityFlags8) & 0x10) == 0)
            At(at::kRoundFlags)[0] = static_cast<unsigned char>(At(at::kRoundFlags)[0] | 0x40);
    }
    g.clear_actor_bit(Sc()[5]);
    SetULong(Enemy() + 0x110, ULong(Enemy() + 0x110) & 0xFFFFFF8Fu);
    SetULong(Enemy() + 0x110, ULong(Enemy() + 0x110) & ~0x200u);
    Sc()[1] = 2;
    Sc()[2] = 0;
    Sc()[3] = 0;
}

// ===========================================================================
// The death
// ===========================================================================

// original 0x436DB0 (PSX 0x801E46FC): the swell's start. Sprite_Current
// +0x48 = 2, the scales +0x44 / +0x40 = 0x10000, the speed +0xC = 0, +0x18 =
// 0x666; +3 = 1.
extern "C" void __cdecl EnemyOp_DeathSwellStart(void) {
    Sc()[0x48] = 2;
    SetULong(Sc() + 0x44, 0x10000);
    SetULong(Sc() + 0x40, 0x10000);
    SetULong(Sc() + 0xC, 0);
    SetULong(Sc() + 0x18, 0x666);
    Sc()[3] = 1;
}

// original 0x436E00 (PSX 0x801E4738): the swell, a frame - EnemyOp_ScaleInStep
// to 0x14000 (1.25) and without the tick; at the end +3 = 2.
extern "C" void __cdecl EnemyOp_DeathSwell(void) {
    unsigned char* const s = Sc();
    const std::uint32_t scale = ULong(s + 0x40);
    if (static_cast<std::int32_t>(scale) < 0x14000) {
        SetULong(s + 0x40, ULong(s + 0xC) + scale);
        SetULong(Sc() + 0x44, ULong(Sc() + 0x44) + ULong(Sc() + 0xC));
        SetULong(Sc() + 0xC, ULong(Sc() + 0xC) + ULong(Sc() + 0x18));
    } else {
        s[3] = 2;
    }
}

// original 0x436E40 (PSX 0x801E4794): the flash. Sound_PlayEffect(0x204); in
// an event battle the cue 0x437450(the word +6 at the enemy's +0xF8);
// Sprite_ReleaseTint(Sprite_Current); Sprite_SetTint(Sprite_Current, 0xFF,
// 0xFF, 0xFF, 0); the speed +0x10 = 0 and its step +0x1C = -0x2000; +3 = 3.
extern "C" void __cdecl EnemyOp_DeathFlash(void) {
    g.play_effect(0x204);
    if (At(at::kEventBattle)[0] != 0) g.play_cue(Word(PtrIn(Enemy() + 0xF8) + 6));
    g.release_tint(Sc());
    g.set_tint(Sc(), 0xFF, 0xFF, 0xFF, 0);
    SetULong(Sc() + 0x10, 0);
    SetULong(Sc() + 0x1C, 0xFFFFE000u);
    Sc()[3] = 3;
}

// original 0x436EC0 (PSX 0x801E482C): the squash, a frame. The scale +0x44
// += the speed +0x10, the speed += +0x1C; once the s32 scale is below 0, a
// tail jump to Battle_EnemyDefeated.
extern "C" void __cdecl EnemyOp_DeathSquash(void) {
    SetULong(Sc() + 0x44, ULong(Sc() + 0x44) + ULong(Sc() + 0x10));
    SetULong(Sc() + 0x10, ULong(Sc() + 0x10) + ULong(Sc() + 0x1C));
    if (Long(Sc() + 0x44) < 0) g.defeated();
}

// ===========================================================================

// original 0x437420 (PSX 0x801E51C8): step 11. Animation 4; then with bit 4
// of the enemy's +0x110 a tail jump to BattleEnemy_ScriptTick, else with
// bit 5 to BattleEnemy_ScriptTickOnce, else nothing.
//
// As the original has it: 0x939AD8 is read after the call.
extern "C" void __cdecl EnemyOp_HitPose(void) {
    g.set_animation(4);
    const std::uint32_t flags = ULong(Enemy() + 0x110);
    if ((flags & 0x10) != 0) g.script_tick();
    else if ((flags & 0x20) != 0) g.script_tick_once();
}

// ===========================================================================

void EnemyAiOps_Inject() {
    if (bof3::WantsShadow("enemy_ai_ops")) enemy_ai_ops::SelfTest();
    BOF3_INJECT(EnemyOp_StepDispatch);
    BOF3_INJECT(EnemyOp_Begin);
    BOF3_INJECT(EnemyOp_EnterDispatch);
    BOF3_INJECT(EnemyOp_ScaleInDispatch);
    BOF3_INJECT(EnemyOp_ScaleInStart);
    BOF3_INJECT(EnemyOp_ScaleInStep);
    BOF3_INJECT(EnemyOp_Idle);
    BOF3_INJECT(EnemyOp_Wait);
    BOF3_INJECT(EnemyOp_HighlightOn);
    BOF3_INJECT(EnemyOp_HighlightPulse);
    BOF3_INJECT(EnemyOp_ActDispatch);
    BOF3_INJECT(EnemyOp_ActBegin);
    BOF3_INJECT(EnemyOp_HitDispatch);
    BOF3_INJECT(EnemyOp_ReceiveAction);
    BOF3_INJECT(EnemyOp_WaitAnimOnce);
    BOF3_INJECT(EnemyOp_HitEnd);
    BOF3_INJECT(EnemyOp_DeathDispatch);
    BOF3_INJECT(EnemyOp_DeathSwellStart);
    BOF3_INJECT(EnemyOp_DeathSwell);
    BOF3_INJECT(EnemyOp_DeathFlash);
    BOF3_INJECT(EnemyOp_DeathSquash);
    BOF3_INJECT(EnemyOp_HitPose);
}

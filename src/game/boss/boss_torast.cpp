// Moved here by tools/move_funcs.py from boss_h.cpp, boss_sb.cpp (docs/source-layout-round.md).
#include "game/boss/boss_torast.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/boss/boss_torast_callees.h"
#include "game/boss_h_callees.h"
#include "game/boss_harness.h"
#include "game/boss_sb_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_torast::at;

}  // namespace

namespace {

using U = std::uint32_t;

using boss_harness::Handler;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

unsigned char* Enemy() { return reinterpret_cast<unsigned char*>(BattleE3_CurrentEnemy.Native()); }

// A named .data table's address (symbols.gen.h binds the name to a typed pointer).
template <typename T> U AddressOf(T* p) { return Low32(p); }

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, a
// Fatal past them (the original jumps through whatever follows).
void Dispatch(const char* who, U table, unsigned entries, unsigned at) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_h.md section 6)",
                    who, at, state, entries, (unsigned)table);
    // the entry as read: the fuzz swaps the table's cells for its recorders
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * state)))))();
}

// Kinds 8..11's colours: three bytes a kind from BossTorast_Colours (kind 8's
// at its start); the original indexes by the kind unchecked, so another kind
// reads the .data around it.
const unsigned char* KindColour(unsigned kind) { return At(AddressOf(BossTorast_Colours) + 3 * (kind - 8)); }

// A table entry as the original's jmp reaches it: the word its caller left on
// the stack, eax back.
using Entry = unsigned long (__cdecl*)(unsigned);

unsigned long CallEntry(U table, unsigned index, unsigned word) {
    // the entry as read: the fuzz swaps the table's cells for its recorders
    return reinterpret_cast<Entry>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * index)))))(word);
}

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, a
// Fatal past them (the original jumps through the dword after).
unsigned long DispatchBossSb(const char* who, U table, unsigned entries, unsigned at, unsigned word) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sb.md section 6)",
                    who, at, state, entries, (unsigned)table);
    return CallEntry(table, state, word);
}

// mov eax, [esp + 4]; and eax, 0xFF; jmp [table + 4 * eax]: an enemy's +0xF4
// hook by its word's low byte (3 entries in every table of the group), the
// word itself left for the entry.
unsigned long HookDispatchBossSb(const char* who, U table, unsigned word) {
    const unsigned index = word & 0xFF;
    if (index >= 3)
        bof3::Fatal("%s: hook word %u is past the 3 entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sb.md section 6)",
                    who, index, (unsigned)table);
    return CallEntry(table, index, word);
}

// A kind's state 0: the enemy's (0x939AD8's) +0xFC animation table, +0xF4
// hook and +0xF8 table, then Sprite_Current +1 = 2 and Sprite_ScriptTick.
void StoreTables(U fc, U hook, U f8) {
    rv::As<records::BattleEnemy>(Enemy())->anim_table = static_cast<std::int32_t>(fc);
    rv::As<records::BattleEnemy>(Enemy())->event_hook = static_cast<std::int32_t>(hook);
    rv::As<records::BattleEnemy>(Enemy())->cue_table = static_cast<std::int32_t>(f8);
}

unsigned char Enter(U fc, U hook, U f8) {
    StoreTables(fc, hook, f8);
    Sprite_Current[1] = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

}  // namespace

// original 0x438E50: BossKind_Table[8]: by +1 through BossTorast_Steps (12).
extern "C" unsigned long __cdecl BossTorast_Dispatch(unsigned word) {
    return DispatchBossSb("BossTorast_Dispatch", AddressOf(BossTorast_Steps), 12, 1, word);
}

// original 0x438E70: kind 8's state 0: +0xFC = BossTorast_Anims, +0xF4 =
// BossTorast_Hook, +0xF8 = BossTorast_Cues; +1 = 2; Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossTorast_Enter(void) {
    return Enter(AddressOf(BossTorast_Anims), ::bof3::Code(bof3::addr::BossTorast_Hook), AddressOf(BossTorast_Cues));
}

// original 0x438EB0: entry 6 of kinds 8..11's +1 tables: by Sprite_Current +2
// through BossTorast_ActSubs (6: EnemyOp_ActBegin, EnemyOp_HitDispatch,
// EnemyOp_ActBegin, EnemyOp_Act3Dispatch, BossTorast_DeathDispatch, EnemyOp_Act5Dispatch) - the
// generic EnemyOp_ActSubs but for its death, entry 4.
extern "C" void __cdecl BossTorast_ActDispatch(void) {
    Dispatch("BossTorast_ActDispatch", AddressOf(BossTorast_ActSubs), 6, 2);
}

// original 0x438ED0: BossTorast_ActSubs 4: by +3 through BossTorast_DeathSubs
// (2: BossTorast_DeathFxDispatch, BossOp_ScriptTick).
extern "C" void __cdecl BossTorast_DeathDispatch(void) {
    Dispatch("BossTorast_DeathDispatch", AddressOf(BossTorast_DeathSubs), 2, 3);
}

// original 0x438EF0: BossTorast_DeathSubs 0: by +4 through
// BossTorast_DeathFxSteps (4: Start, Flash, RingGrow, RingShrink).
extern "C" void __cdecl BossTorast_DeathFxDispatch(void) {
    Dispatch("BossTorast_DeathFxDispatch", AddressOf(BossTorast_DeathFxSteps), 4, 4);
}

// original 0x438F10: step 0: +9 and +0xA = 0x40, Sound_PlayEffect(0x601), and
// +4 (Sprite_Current read after the call) up by one.
extern "C" void __cdecl BossTorast_DeathFxStart(void) {
    unsigned char* const s = Sprite_Current;
    s[9] = 0x40;
    s[0xA] = 0x40;
    BH_CALL(Sound_PlayEffect)(0x601);
    Sprite_Current[4] += 1;
}

// original 0x438F40: step 1: when the count +9 has reached the interval +0xA,
// a screen tile in the kind's colour - BattleWin_DrawTileRgb(0, 0, 0xB, the
// 15-bit colour of 0x939AD8's kind +0x100 in BossTorast_Colours, 1) - and the
// interval halved (on Sprite_Current read after the call); otherwise +9 down
// by one. Then +4 up by one when +9 (read again) is 0.
extern "C" void __cdecl BossTorast_DeathFxFlash(void) {
    unsigned char* const s = Sprite_Current;
    if (s[9] == s[0xA]) {
        const unsigned char* const c = KindColour(rv::As<records::BattleEnemy>(Enemy())->run_state);
        const U colour = (static_cast<U>(c[0] & 0xF8) << 7) + (static_cast<U>(c[1] & 0xF8) << 2) + (c[2] >> 3);
        BH_CALL(BattleWin_DrawTileRgb)(0, 0, 0xB, static_cast<int>(colour), 1);
        Sprite_Current[0xA] >>= 1;
    } else {
        s[9] = static_cast<unsigned char>(s[9] - 1);
    }
    unsigned char* const t = Sprite_Current;
    if (t[9] == 0) t[4] += 1;
}

// original 0x438FD0: step 2: the ring at radius +9; then (Sprite_Current read
// after the call) at +9 == 0x30: +0x48 = 2, the dwords +0x40 and +0x44 =
// 0x10000, Sound_PlayEffect(0x602) and +4 (read after) up by one; otherwise
// +9 up by 4.
extern "C" void __cdecl BossTorast_DeathFxRingGrow(void) {
    BH_CALL(BossTorast_DrawRing)(Sprite_Current[9]);
    unsigned char* const s = Sprite_Current;
    if (s[9] != 0x30) {
        s[9] = static_cast<unsigned char>(s[9] + 4);
        return;
    }
    s[0x48] = 2;
    SetLong(s + 0x40, 0x10000);
    SetLong(s + 0x44, 0x10000);
    BH_CALL(Sound_PlayEffect)(0x602);
    Sprite_Current[4] += 1;
}

// original 0x439030: step 3: the ring at radius +9; then (Sprite_Current read
// after the call) at +9 == 0: +0x48 = 0, Sprite_SetAnimationBank(0x83),
// Sprite_SetAnimation(0), Battle_EnemyDefeated, and on Sprite_Current read
// after them +0 &= 0xBF, +1 = 6, +2 = 4, +3 up by one, +4 = 0. Otherwise +9
// down by 4 and the dwords +0x40 and +0x44 each down by 0x2000 while above 0
// (signed), else 0.
extern "C" void __cdecl BossTorast_DeathFxRingShrink(void) {
    BH_CALL(BossTorast_DrawRing)(Sprite_Current[9]);
    unsigned char* s = Sprite_Current;
    if (s[9] == 0) {
        s[0x48] = 0;
        BH_CALL(Sprite_SetAnimationBank)(0x83);
        BH_CALL(Sprite_SetAnimation)(0);
        BH_CALL(Battle_EnemyDefeated)();
        s = Sprite_Current;
        s[0] &= 0xBF;
        s[1] = 6;
        s[2] = 4;
        s[3] += 1;
        s[4] = 0;
        return;
    }
    s[9] = static_cast<unsigned char>(s[9] - 4);
    for (const unsigned off : {0x40u, 0x44u}) {
        const std::int32_t v = Long(s + off);
        SetLong(s + off, v > 0 ? static_cast<std::int32_t>(static_cast<U>(v) + 0xFFFFE000u) : 0);
    }
}

// original 0x4390E0: kind 8's +0xF4 hook through BossTorast_Hooks (3, BareRet).
extern "C" unsigned long __cdecl BossTorast_Hook(unsigned word) {
    return HookDispatchBossSb("BossTorast_Hook", AddressOf(BossTorast_Hooks), word);
}

void BossTorast_Inject() {
    BOF3_INJECT(BossTorast_Dispatch);
    BOF3_INJECT(BossTorast_Enter);
    BOF3_INJECT(BossTorast_ActDispatch);
    BOF3_INJECT(BossTorast_DeathDispatch);
    BOF3_INJECT(BossTorast_DeathFxDispatch);
    BOF3_INJECT(BossTorast_DeathFxStart);
    BOF3_INJECT(BossTorast_DeathFxFlash);
    BOF3_INJECT(BossTorast_DeathFxRingGrow);
    BOF3_INJECT(BossTorast_DeathFxRingShrink);
    BOF3_INJECT(BossTorast_Hook);
}

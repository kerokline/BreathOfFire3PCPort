// The boss band's shared helpers: the 20 functions of 0x437CA0..0x440829 that
// three or more of the band's units reach (tools/boss_rows.py --unit H,
// 2026-09-28; analysis/boss_funcs.tsv's group column BH), each read to its
// last instruction with capstone (2026-09-28) and taken through the boss
// harness (boss_harness.h). Round eleven group BH; docs/boss_h.md has them one
// row each.
//
//   - two fillers: BareRet (the bare ret in 455 hook and table slots,
//     engine-wide) and BareRetZero (an event hook answering al 0);
//   - four hooks: the end hook that picks the way out, two exit hooks that
//     clear the actor tagged 0, and an enemy hook that retargets;
//   - the state helpers several kinds' tables share: tick the sprite's
//     script, tick it once then state 2, the death (Battle_EnemyDefeated and
//     the state bytes);
//   - kinds 8..11's shared death sequence (Torast, Kassen, Galtel, Doksen -
//     the tool's names): three dispatchers by +2, +3, +4 over their shared
//     tables, then four steps - a flash of the screen in the kind's colour at
//     halving intervals, a ring that grows, a ring that shrinks and the
//     defeat - and the ring's draw (64 Gouraud triangles);
//   - the area-79 fights' map helpers: a 2 x 2 block of the map's corner
//     cells set, the block and the flags by two enemies' status, a byte by
//     the leader's character id.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// three dispatchers and BossMap_SetCorners abort past their tables where the
// original would jump or read through whatever follows (the owner's rule for
// an unchecked index, round9 doc section 6; no route reaches it). Every call
// goes through the harness (BH_CALL / BH_AT / Phase), so the start-up fuzz can
// stand recorders in for the callees.
#include "game/boss_h.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/boss_h_callees.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_h::at;
using U = std::uint32_t;
using boss_harness::Handler;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::Word;

unsigned char& B(U address) { return At(address)[0]; }
unsigned char* Enemy() { return At(static_cast<U>(Long(At(at::kCurrentEnemy)))); }
std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
void PutFloat(unsigned char* p, std::int32_t v) {
    const float f = static_cast<float>(v);   // fild / fstp: nearest, as cvtsi2ss
    std::memcpy(p, &f, sizeof f);
}
// A named .data table's address (symbols.gen.h binds the name to a typed pointer).
template <typename T> U AddressOf(T* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }

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

}  // namespace

// --- the fillers -----------------------------------------------------------------

// original 0x437CC0: a bare ret. Boss_SetupTable[0], every hook slot a set-up
// leaves empty, and 455 references engine-wide (the effect dispatchers' slot
// 0, the kinds' tables, field tables).
extern "C" void __cdecl BareRet(void) {}

// original 0x43C9F0: xor al, al; ret - the event hook (0x904B6C) of 39
// set-ups: al 0, so BattleRoundEnd_NextRound's "al 0xFF holds the round"
// never holds. The original clears al alone; ours all of eax (callers read al).
extern "C" unsigned char __cdecl BareRetZero(unsigned) { return 0; }

// --- the hooks ---------------------------------------------------------------------

// original 0x43EB60: the end hook (0x904B64) of ten set-ups. Won (0x904AE8
// bit 1): the chapter's step 0x8034E5 = 5 and a tail jump to 0x446DE0 (the end
// phase, step 1); otherwise to 0x446E00 (step 2).
extern "C" void __cdecl BossHook_EndPickWay(void) {
    if (B(at::kBattleEnd) & 2) {
        B(at::kChapterStep) = 5;
        BH_AT(Handler, at::kEndWin)();
        return;
    }
    BH_AT(Handler, at::kEndOther)();
}

// original 0x43A4A0: the exit hook (0x904B68) of set-ups 14, 28, 29:
// BossActor_Clear(0).
extern "C" void __cdecl BossHook_ExitClearActor0(void) { BH_CALL(BossActor_Clear)(0); }

// original 0x440820: the exit hook of set-ups 3, 17, 55: BossActor_ClearBit40(0).
extern "C" void __cdecl BossHook_ExitActor0Bit40(void) { BH_CALL(BossActor_ClearBit40)(0); }

// original 0x43A750: entry 0 of kinds 21, 22 and 23's +0xF4 hook tables (the
// hook called with 0, 0x435BF5): when the leader's target byte +0x124 is 4,
// the battle's target 0x904B44 = 5. The hook's word is not read.
extern "C" void __cdecl BossHook_RetargetMember0(unsigned) {
    if (B(at::kLeaderTarget) == 4) B(at::kTarget) = 5;
}

// --- the state helpers ---------------------------------------------------------------

// original 0x437CA0: jmp Sprite_ScriptTick - a state entry of kinds 3, 6, 7,
// 40 and 8..11 (BossTorast_DeathSubs 1): the sprite's script ticked, its al
// the answer.
extern "C" unsigned char __cdecl BossOp_ScriptTick(void) { return BH_CALL(Sprite_ScriptTick)(); }

// original 0x43A720: state 1 of kinds 21, 22 and 27 (their +1 tables' entry
// 1): Sprite_ScriptTickOnce, and when it answers al not 0, Sprite_Current's
// +1 (read after the call) = 2.
extern "C" void __cdecl BossOp_EnterTick(void) {
    if (BH_CALL(Sprite_ScriptTickOnce)() & 0xFF) Sprite_Current[1] = 2;
}

// original 0x43B550: entry 4 (the death) of the +2 tables of kinds 2, 3, 6, 7,
// 18 and 27: Sprite_ScriptTickOnce (its answer not read), Battle_EnemyDefeated,
// then 0x939AD8's +0x110 |= 0x1000, and Sprite_Current (read after the calls)
// +0 &= 0xBF, +1 = 3, +2 = 0, +3 = 0.
extern "C" void __cdecl BossOp_Death(void) {
    BH_CALL(Sprite_ScriptTickOnce)();
    BH_CALL(Battle_EnemyDefeated)();
    unsigned char* const e = Enemy();
    SetLong(e + 0x110, static_cast<std::int32_t>(static_cast<U>(Long(e + 0x110)) | 0x1000));
    unsigned char* const s = Sprite_Current;
    s[0] &= 0xBF;
    s[1] = 3;
    s[2] = 0;
    s[3] = 0;
}

// --- kinds 8..11's death ---------------------------------------------------------------

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
        const unsigned char* const c = KindColour(Enemy()[0x100]);
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

// original 0x4394A0: the ring of radius `radius` (its low byte) round
// Sprite_Current's screen point (+0x2E, +0x30): 64 points (Math_Sin / Math_Cos
// of i * 0x80, times the radius, >> 12 - two laps of the 4096-step circle);
// a draw mode (Gpu_GetTPage(0, 0, 0x3C0, 0), Gpu_SetDrawMode(prim, 0, 0,
// tpage & 0xFFFF, 0), Gfx_CommitPrim(3, 0xC)); then 64 Gouraud triangles at
// Gfx_PacketNext (read for each): Gpu_SetPolyG3, the centre white, the rim's
// two corners in the kind's colour (0x939AD8's +0x100 in BossTorast_Colours),
// the corners as floats - the centre, the centre + point i, the centre + point
// (i + 1) & 31 (the first lap's: the second lap draws over it) - read after
// the call, Gpu_SetSemiTrans(prim, 1), Gfx_CommitPrim(3, 0x34).
extern "C" void __cdecl BossTorast_DrawRing(unsigned radius) {
    const U r = radius & 0xFF;
    std::int32_t pts[64][2];
    for (unsigned i = 0; i < 64; ++i) {
        const int angle = static_cast<int>(i * 0x80);
        pts[i][0] = static_cast<std::int32_t>(static_cast<U>(BH_CALL(Math_Sin)(angle)) * r) >> 12;
        pts[i][1] = static_cast<std::int32_t>(static_cast<U>(BH_CALL(Math_Cos)(angle)) * r) >> 12;
    }
    const unsigned tpage = BH_CALL(Gpu_GetTPage)(0, 0, 0x3C0, 0);
    BH_CALL(Gpu_SetDrawMode)(At(static_cast<U>(Long(At(at::kPacketNext)))), 0, 0, tpage & 0xFFFF, 0);
    BH_CALL(Gfx_CommitPrim)(3, 0xC);
    for (unsigned i = 0; i < 64; ++i) {
        unsigned char* const p = At(static_cast<U>(Long(At(at::kPacketNext))));
        BH_CALL(Gpu_SetPolyG3)(p);
        p[4] = 0xFF;
        p[5] = 0xFF;
        p[6] = 0xFF;
        const unsigned char* const c = KindColour(Enemy()[0x100]);
        p[0x14] = c[0];
        p[0x15] = c[1];
        p[0x16] = c[2];
        p[0x24] = c[0];
        p[0x25] = c[1];
        p[0x26] = c[2];
        const unsigned char* const s = Sprite_Current;
        const std::int32_t x = S16(s + 0x2E), y = S16(s + 0x30);
        const unsigned j = (i + 1) & 0x1F;
        PutFloat(p + 0x08, x);
        PutFloat(p + 0x0C, y);
        PutFloat(p + 0x18, static_cast<std::int32_t>(static_cast<U>(x) + static_cast<U>(pts[i][0])));
        PutFloat(p + 0x1C, static_cast<std::int32_t>(static_cast<U>(y) + static_cast<U>(pts[i][1])));
        PutFloat(p + 0x28, static_cast<std::int32_t>(static_cast<U>(x) + static_cast<U>(pts[j][0])));
        PutFloat(p + 0x2C, static_cast<std::int32_t>(static_cast<U>(y) + static_cast<U>(pts[j][1])));
        BH_CALL(Gpu_SetSemiTrans)(p, 1);
        BH_CALL(Gfx_CommitPrim)(3, 0x34);
    }
}

// --- the area-79 fights' map helpers ---------------------------------------------------------

// original 0x43B0D0: the map's corner cells of a 2 x 2 block set to one dword:
// the block's corner (x, y) the bytes BossMap_CornerCells[cell] (2 pairs), the
// dword BossMap_CornerValues[value] (3); cell (x + i, y + j) is AreaMap_Corners
// [width * (y + j) + x + i], width the byte AreaMap_Header +0 (read for each).
// Called by kind 26's 0x43AA70 (value 2) and BossMap_UpdateFromEnemies (0 or 1).
extern "C" void __cdecl BossMap_SetCorners(unsigned cell, unsigned value) {
    if (cell > 1 || value > 2)
        bof3::Fatal("BossMap_SetCorners(%u, %u): past BossMap_CornerCells (2) or BossMap_CornerValues (3) - the "
                    "original reads whatever follows (docs/boss_h.md section 6)",
                    cell, value);
    const std::int32_t v = Long(At(AddressOf(BossMap_CornerValues) + 4 * value));
    const U x = At(AddressOf(BossMap_CornerCells) + 2 * cell)[0];
    const U y = At(AddressOf(BossMap_CornerCells) + 2 * cell + 1)[0];
    for (U j = 0; j < 2; ++j)
        for (U i = 0; i < 2; ++i) {
            const U width = At(at::kMapHeader)[0];
            SetLong(At(at::kMapCorners + 4 * (width * (y + j) + x + i)), v);
        }
}

// original 0x43B130: by the leader's character id 0x802DC9 - 0, 1, 5, 6 - the
// byte 0x92BF18 = 0x2C, 0x2D, 0x2E, 0x2F (a jump table of 7 inside the
// function); any other id leaves it. Called by set-ups 18..20's hooks before
// the battle ends.
extern "C" void __cdecl Boss_SetByLeaderId(void) {
    switch (B(at::kLeaderId)) {
    case 0: B(at::kLeaderPick) = 0x2C; break;
    case 1: B(at::kLeaderPick) = 0x2D; break;
    case 5: B(at::kLeaderPick) = 0x2E; break;
    case 6: B(at::kLeaderPick) = 0x2F; break;
    default: break;
    }
}

// original 0x43B180: called by set-ups 18..20's event hooks (phase 5). For
// enemy 1 (0x93BA88) and then enemy 2 (0x93BBB0), unless its +0x93 has 0x40:
// with 0x20 - flag 0x34 (0x33 for enemy 2) of the chapter's bits set,
// Sprite_Current = the enemy, Sprite_EnsureAnimation(3), BossMap_SetCorners(1
// (0 for enemy 2), 1); without - the flag cleared, Sprite_EnsureAnimation(2),
// BossMap_SetCorners(.., 0). Then enemy 0's ground word +0x3E and the
// leader's = AreaMap_Elevation of their (+0x34, +0x38); and with the leader's
// +0x134 bit 1, 0x939B1C = its +0x3C.
extern "C" void __cdecl BossMap_UpdateFromEnemies(void) {
    const struct { U status, object; unsigned flag, cell; } kEnemies[] = {
        {at::kEnemy1Status, at::kEnemy1, 0x34, 1},
        {at::kEnemy2Status, at::kEnemy2, 0x33, 0},
    };
    for (const auto& e : kEnemies) {
        const unsigned status = At(e.status)[1];   // the dword's second byte: +0x93
        if (status & 0x40) continue;
        const bool up = (status & 0x20) != 0;
        unsigned char* const bits = At(static_cast<U>(Long(At(at::kFlagBits))));
        if (up) BH_CALL(Flags_Set)(bits, e.flag);
        else BH_CALL(Flags_Clear)(bits, e.flag);
        Sprite_Current = At(e.object);
        BH_CALL(Sprite_EnsureAnimation)(up ? 3 : 2);
        BH_CALL(BossMap_SetCorners)(e.cell, up ? 1 : 0);
    }
    const long ground = BH_CALL(AreaMap_Elevation)(Long(At(at::kEnemy0X)), Long(At(at::kEnemy0Z)));
    move_script::SetWord(At(at::kEnemy0Ground), static_cast<unsigned>(ground) & 0xFFFF);
    const long leader = BH_CALL(AreaMap_Elevation)(Long(At(at::kLeaderX)), Long(At(at::kLeaderZ)));
    move_script::SetWord(At(at::kLeaderGround), static_cast<unsigned>(leader) & 0xFFFF);
    if (B(at::kLeaderFlags) & 2) SetLong(At(at::kCameraY), Long(At(at::kLeaderY)));
}

void BossH_Inject() {
    if (bof3::WantsShadow("boss_h")) boss_h::SelfTest();
    BOF3_INJECT(BossOp_ScriptTick);
    BOF3_INJECT(BareRet);
    BOF3_INJECT(BossTorast_ActDispatch);
    BOF3_INJECT(BossTorast_DeathDispatch);
    BOF3_INJECT(BossTorast_DeathFxDispatch);
    BOF3_INJECT(BossTorast_DeathFxStart);
    BOF3_INJECT(BossTorast_DeathFxFlash);
    BOF3_INJECT(BossTorast_DeathFxRingGrow);
    BOF3_INJECT(BossTorast_DeathFxRingShrink);
    BOF3_INJECT(BossTorast_DrawRing);
    BOF3_INJECT(BossHook_ExitClearActor0);
    BOF3_INJECT(BossOp_EnterTick);
    BOF3_INJECT(BossHook_RetargetMember0);
    BOF3_INJECT(BossMap_SetCorners);
    BOF3_INJECT(Boss_SetByLeaderId);
    BOF3_INJECT(BossMap_UpdateFromEnemies);
    BOF3_INJECT(BossOp_Death);
    BOF3_INJECT(BareRetZero);
    BOF3_INJECT(BossHook_EndPickWay);
    BOF3_INJECT(BossHook_ExitActor0Bit40);
}

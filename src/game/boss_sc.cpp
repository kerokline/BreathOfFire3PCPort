// Round eleven group BSC (docs/takeover-queue-bosses.md section 3): the boss
// band's units B11, K12, B12, K13, K14, K17, B16, K15, K53, B14, B46, K16 and
// B15 - 53 functions of 0x439410..0x43A589 (tools/boss_rows.py, 2026-09-28;
// analysis/boss_funcs.tsv's group column BSC), each read to its last
// instruction with capstone (2026-09-28) and taken through the boss harness
// (boss_harness.h). Enemy names are tools/boss_rows.py --disc's (the US
// disc's area records), not memory of the game; docs/boss_sc.md has every
// function one row each.
//
//   - six set-ups (fights 11, 12, 14, 15, 16, 46): the three hooks stored, and
//     fight 16's wait count drawn from Rand;
//   - their end hooks (the move-script counter 0x903848 and the way out by
//     the win bit), their exit hooks (the field actors found by tag given
//     their poses back), fight 16's event hook (a line by the phase code);
//   - seven kinds' dispatchers by the state byte +1, three second-level
//     dispatchers (+2, +3), seven +0xF4 hook tables by the hook's word;
//   - the kinds' entrances (state 0: the +0xF4 hook and the +0xF8 / +0xFC
//     tables stored), Amalgam's death (four steps and two draws), the
//     Balio / Sunder / Nina hooks of fights 13 and 16.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers abort past their tables where the original jumps through
// whatever follows (the owner's rule for an unchecked index, round9 doc
// section 6); the exit hooks abort where the original would write through a
// null actor (unreachable: the same tag's BossActor_ClearBit40 aborts first).
// Every call goes through the harness (BH_CALL / BH_AT), so the start-up fuzz
// can stand recorders in for the callees.
#include "game/boss_sc.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sc_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_sc::at;
using U = std::uint32_t;
using boss_harness::Handler;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

constexpr U kCurrentEnemy = 0x939AD8;
constexpr U kHookEnd = 0x904B64, kHookExit = 0x904B68, kHookEvent = 0x904B6C;
constexpr U kPacketNext = 0x7E0670;

unsigned char& B(U address) { return At(address)[0]; }
unsigned char* Enemy() { return At(static_cast<U>(Long(At(kCurrentEnemy)))); }
unsigned char* Packet() { return At(static_cast<U>(Long(At(kPacketNext)))); }
std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
U L(const unsigned char* p) { return static_cast<U>(Long(p)); }
void PutFloat(unsigned char* p, U v) {
    const float f = static_cast<float>(static_cast<std::int32_t>(v));   // fild / fstp: nearest
    std::memcpy(p, &f, sizeof f);
}

// The three hooks a set-up stores (BattleHook_End / _Exit / _Event).
void StoreHooks(U end, U exit, U event) {
    SetLong(At(kHookEnd), static_cast<std::int32_t>(end));
    SetLong(At(kHookExit), static_cast<std::int32_t>(exit));
    SetLong(At(kHookEvent), static_cast<std::int32_t>(event));
}

// A kind's entrance: 0x939AD8's +0xFC (its animation bytes), +0xF4 (its hook)
// and +0xF8, in that order, 0x939AD8 read before each (no call between).
void StoreKindTables(U fc, U f4, U f8) {
    SetLong(Enemy() + 0xFC, static_cast<std::int32_t>(fc));
    SetLong(Enemy() + 0xF4, static_cast<std::int32_t>(f4));
    SetLong(Enemy() + 0xF8, static_cast<std::int32_t>(f8));
}

using Entry = U (__cdecl*)(U);
Entry EntryAt(U table, unsigned index) { return reinterpret_cast<Entry>(static_cast<std::uintptr_t>(L(At(table + 4 * index)))); }

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, a
// Fatal past them (the original jumps through whatever follows). The jmp
// leaves the stack as the dispatcher found it, so the entry sees the word
// above the return address that the dispatcher saw: ours hands it on (an
// entry that reads it - Port_DroppedCall, a bare ret with a byte - sees what
// it sees under the original), and answers the entry's eax.
U Dispatch(const char* who, U table, unsigned entries, unsigned at, U through) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sc.md section 7)",
                    who, at, state, entries, (unsigned)table);
    return EntryAt(table, state)(through);   // the entry as read: the fuzz swaps the table's cells for its recorders
}

// An enemy's +0xF4 hook table: jmp [table + 4 * (word & 0xFF)], 3 entries.
U HookDispatch(const char* who, U table, U word) {
    const unsigned index = word & 0xFF;
    if (index >= 3)
        bof3::Fatal("%s: the hook's word is 0x%X, past the 3 entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sc.md section 7)",
                    who, (unsigned)word, (unsigned)table);
    return EntryAt(table, index)(word);
}

// BossActor_Find(tag) for a hook that writes through the answer: the original
// uses it untested and faults near address 0 when no field actor carries the
// tag. Unreachable after the same tag's BossActor_ClearBit40 (ours aborts
// there first); ours aborts here too rather than write through null.
unsigned char* Actor(const char* who, unsigned tag) {
    unsigned char* const a = BH_CALL(BossActor_Find)(tag);
    if (a == nullptr)
        bof3::Fatal("%s: no field actor carries tag %u - the original writes through a null pointer (docs/boss_sc.md "
                    "section 7)",
                    who, tag);
    return a;
}

// Fights 13 and 16's line: the enemy's action byte, the window pass byte 2,
// then Msg_OpenScript(line).
void Line(unsigned char action, unsigned short line) {
    B(at::kAction) = action;
    B(at::kMsgPass) = 2;
    BH_CALL(Msg_OpenScript)(line);
}

// The banner with a character's name (fight 16's event hook and Nina's hit
// hook share it): Battle_OpenMsgWindow, Str_CopyN(Text_Records, the live
// record 0x66972D names, 8), BattleBanner_Add(2, 0, 0, 0x2D,
// Msg_SystemPtr(0x25)).
void NameBanner() {
    BH_CALL(Battle_OpenMsgWindow)();
    const unsigned n = B(at::kBannerChar);
    BH_CALL(Str_CopyN)(reinterpret_cast<char*>(At(bof3::addr::Text_Records)),
                       reinterpret_cast<const char*>(At(at::kCharRecords + n * at::kCharStride)), 8);
    const unsigned char* const text = BH_CALL(Msg_SystemPtr)(0x25);
    BH_CALL(BattleBanner_Add)(2, 0, 0, 0x2D, reinterpret_cast<const char*>(text));
}

// The end hooks' common shape: on the win (0x904AE8 bit 1) the move-script
// counter byte 0x903848 = scene and the end phase's step 1, else step 2.
void EndByWin(unsigned char scene) {
    if (B(at::kBattleEnd) & 2) {
        B(at::kMoveCounter) = scene;
        BH_AT(Handler, at::kEndWin)();
        return;
    }
    BH_AT(Handler, at::kEndOther)();
}

}  // namespace

// ============================================================================
// Fight 11 (BOSS008, area 27 row 4)
// ============================================================================

// original 0x439410: Boss_SetupTable[11]: BattleHook_End = Boss11_End,
// _Exit = Boss11_Exit, _Event = BareRetZero.
extern "C" void __cdecl Boss11_Setup(void) { StoreHooks(bof3::addr::Boss11_End, bof3::addr::Boss11_Exit, bof3::addr::BareRetZero); }

// original 0x439430: won: 0x903848 = 0x44 and a tail jump to 0x446DE0; else
// to 0x446E00.
extern "C" void __cdecl Boss11_End(void) { EndByWin(0x44); }

// original 0x439450: BossActor_ClearBit40(3); Sprite_Current =
// BossActor_Find(3); Sprite_SetAnimationBank(0x83); Sprite_SetAnimation(0);
// then (Sprite_Current read after the calls) its +0x58 / +0x5A = enemy 0's.
extern "C" void __cdecl Boss11_Exit(void) {
    BH_CALL(BossActor_ClearBit40)(3);
    Sprite_Current = Actor("Boss11_Exit", 3);
    BH_CALL(Sprite_SetAnimationBank)(0x83);
    BH_CALL(Sprite_SetAnimation)(0);
    SetWord(Sprite_Current + 0x58, Word(At(at::kEnemy0 + at::kEnemyPose)));
    SetWord(Sprite_Current + 0x5A, Word(At(at::kEnemy0 + at::kEnemyPose + 2)));
}

// ============================================================================
// Kind 12 (Amalgam, area 28)
// ============================================================================

// original 0x4396B0: BossKind_Table[12]: by +1 through BossAmalgam_Steps (12).
extern "C" unsigned long __cdecl BossAmalgam_Dispatch(unsigned long through) {
    return Dispatch("BossAmalgam_Dispatch", 0x64CC24, 12, 1, through);
}

// original 0x4396D0: state 0: the kind's tables (+0xFC BossAmalgam_Anims,
// +0xF4 BossAmalgam_Hook, +0xF8 BossAmalgam_F8), 0x455290(Sprite_Current,
// BossAmalgam_SlotScripts) - a field slot script for the sprite -, +1 = 2 on
// Sprite_Current read after the call, and a tail jump to Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossAmalgam_Enter(void) {
    StoreKindTables(0x64CBC4, bof3::addr::BossAmalgam_Hook, 0x64CBD0);
    BH_AT(void (__cdecl*)(unsigned char*, U), at::kSlotStart)(Sprite_Current, 0x64CC14);
    Sprite_Current[1] = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x439730: BossAmalgam_Steps 6: by +2 through BossAmalgam_ActSubs
// (6: EnemyOp_ActBegin, EnemyOp_HitDispatch, EnemyOp_ActBegin, EnemyOp_Act3Dispatch,
// BossAmalgam_DeathDispatch, EnemyOp_Act5Dispatch - EnemyOp_ActSubs with the death at 4).
extern "C" unsigned long __cdecl BossAmalgam_ActDispatch(unsigned long through) {
    return Dispatch("BossAmalgam_ActDispatch", 0x64CC54, 6, 2, through);
}

// original 0x439750: BossAmalgam_ActSubs 4: by +3 through BossAmalgam_DeathSteps
// (4: Tick, Start, Melt, End).
extern "C" unsigned long __cdecl BossAmalgam_DeathDispatch(unsigned long through) {
    return Dispatch("BossAmalgam_DeathDispatch", 0x64CC6C, 4, 3, through);
}

// original 0x439770: death step 0: Sprite_ScriptTick; when it answers al not
// 0, +3 = 1 (Sprite_Current read after the call) and eax is that pointer;
// else eax is the tick's answer.
extern "C" unsigned __cdecl BossAmalgam_DeathTick(void) {
    const unsigned char done = BH_CALL(Sprite_ScriptTick)();
    if (done == 0) return done;
    unsigned char* const s = Sprite_Current;
    s[3] = 1;
    return static_cast<U>(reinterpret_cast<std::uintptr_t>(s));
}

// original 0x439790: death step 1: 0x904AA9 |= 4; Sprite_ScriptTickOnce (not
// read); Gfx_ClearRect(0x340, 0x100, 0x80, 0x100); Sound_PlayById(0x601); then
// on Sprite_Current: +0x18 / +0x1C = the words +0x2E / +0x30 sign-extended,
// +0x20 = 0, +0x2E = 0x32, +0x30 = 0x41, +0x24 |= 0x88;
// BossAmalgam_DrawSprite(+0x20's word); +3 = 2 (read after the call).
extern "C" void __cdecl BossAmalgam_DeathStart(void) {
    B(at::kRoundFlagsHi) |= 4;
    BH_CALL(Sprite_ScriptTickOnce)();
    BH_CALL(Gfx_ClearRect)(0x340, 0x100, 0x80, 0x100);
    BH_CALL(Sound_PlayById)(0x601);
    unsigned char* const s = Sprite_Current;
    SetLong(s + 0x18, S16(s + 0x2E));
    SetLong(s + 0x1C, S16(s + 0x30));
    SetLong(s + 0x20, 0);
    SetWord(s + 0x2E, 0x32);
    SetWord(s + 0x30, 0x41);
    s[0x24] |= 0x88;
    BH_CALL(BossAmalgam_DrawSprite)(Word(s + 0x20));
    Sprite_Current[3] = 2;
}

// original 0x439830: death step 2: BossAmalgam_DrawSprite(+0x20's word),
// BossAmalgam_DrawStreak(+0x20's word, Sprite_Current read again), then the
// dword +0x20 up by 2 and at 0x56 +3 = 3.
extern "C" void __cdecl BossAmalgam_DeathMelt(void) {
    BH_CALL(BossAmalgam_DrawSprite)(Word(Sprite_Current + 0x20));
    BH_CALL(BossAmalgam_DrawStreak)(Word(Sprite_Current + 0x20));
    unsigned char* const s = Sprite_Current;
    SetLong(s + 0x20, static_cast<std::int32_t>(L(s + 0x20) + 2));
    if (L(s + 0x20) == 0x56) s[3] = 3;
}

// original 0x439870: death step 3: 0x454A80(Sprite_Current) - its field slots
// released -, Battle_EnemyDefeated, 0x939AD8's +0x110 |= 0x1000, then
// Sprite_Current +0 &= 0xBF, +1 = 3, +2 = 0, +3 = 0.
extern "C" void __cdecl BossAmalgam_DeathEnd(void) {
    BH_AT(void (__cdecl*)(unsigned char*), at::kSlotsReleaseFor)(Sprite_Current);
    BH_CALL(Battle_EnemyDefeated)();
    unsigned char* const e = Enemy();
    SetLong(e + 0x110, static_cast<std::int32_t>(L(e + 0x110) | 0x1000));
    unsigned char* const s = Sprite_Current;
    s[0] &= 0xBF;
    s[1] = 3;
    s[2] = 0;
    s[3] = 0;
}

// original 0x4398D0: the +0xF4 hook: by the word's low byte through
// BossAmalgam_Hooks (3, all BareRet).
extern "C" unsigned long __cdecl BossAmalgam_Hook(unsigned long word) {
    return HookDispatch("BossAmalgam_Hook", 0x64CC7C, word);
}

// original 0x439930: a SPRT of the cleared VRAM rect: Gpu_GetTPage(2, 0,
// 0x340, 0x100); Gpu_SetDrawMode(Gfx_PacketNext, 0, 0, tpage & 0xFFFF, 0);
// Gfx_CommitPrim(1, 0xC); then at Gfx_PacketNext (read after): colour
// 0x80 0x80 0x80, x = +0x18 - 0x28 and y = +0x20 + +0x1C - 0x3C as floats,
// u 0, v the argument's low byte, w 0x68, h 0x56; Gpu_SetSprt, Gfx_CommitPrim(1,
// 0x1C).
extern "C" void __cdecl BossAmalgam_DrawSprite(unsigned v) {
    const U tpage = BH_CALL(Gpu_GetTPage)(2, 0, 0x340, 0x100) & 0xFFFF;
    BH_CALL(Gpu_SetDrawMode)(Packet(), 0, 0, tpage, 0);
    BH_CALL(Gfx_CommitPrim)(1, 0xC);
    unsigned char* const p = Packet();
    p[4] = p[5] = p[6] = 0x80;
    const unsigned char* const s = Sprite_Current;
    PutFloat(p + 8, L(s + 0x18) - 0x28);
    PutFloat(p + 0xC, L(s + 0x20) + L(s + 0x1C) - 0x3C);
    p[0x14] = 0;
    SetWord(p + 0x18, 0x68);
    p[0x15] = static_cast<unsigned char>(v);
    SetWord(p + 0x1A, 0x56);
    BH_CALL(Gpu_SetSprt)(p);
    BH_CALL(Gfx_CommitPrim)(1, 0x1C);
}

// original 0x4399E0: a POLY_FT4 stretching texture row v of the same rect from
// the top of the screen down to the sprite: the draw mode as above
// (Gfx_CommitPrim(1, 0xC)); the quad at Gfx_PacketNext read after that call,
// its tpage word +0x26 a second Gpu_GetTPage(2, 0, 0x340, 0x100); colour 0x80
// 0x80 0x80; corners (+0x18 - 0x28, 0), (+0x18 + 0x40, 0), (+0x18 - 0x28, y),
// (+0x18 + 0x40, y) with y = +0x20 + +0x1C - 0x3C, as floats; u 0 / 0x68 / 0 /
// 0x68, v the argument's low byte at each; Gpu_SetPolyFT4, Gfx_CommitPrim(1,
// 0x48).
extern "C" void __cdecl BossAmalgam_DrawStreak(unsigned v) {
    const U mode = BH_CALL(Gpu_GetTPage)(2, 0, 0x340, 0x100) & 0xFFFF;
    BH_CALL(Gpu_SetDrawMode)(Packet(), 0, 0, mode, 0);
    BH_CALL(Gfx_CommitPrim)(1, 0xC);
    unsigned char* const p = Packet();
    const U tpage = BH_CALL(Gpu_GetTPage)(2, 0, 0x340, 0x100);
    SetWord(p + 0x26, tpage);
    p[4] = p[5] = p[6] = 0x80;
    const unsigned char* const s = Sprite_Current;
    const U left = L(s + 0x18) - 0x28, right = L(s + 0x18) + 0x40, bottom = L(s + 0x20) + L(s + 0x1C) - 0x3C;
    SetLong(p + 0xC, 0);
    PutFloat(p + 8, left);
    SetLong(p + 0x1C, 0);
    PutFloat(p + 0x18, right);
    PutFloat(p + 0x28, left);
    PutFloat(p + 0x2C, bottom);
    PutFloat(p + 0x38, right);
    const auto row = static_cast<unsigned char>(v);
    p[0x14] = 0;
    p[0x34] = 0;
    p[0x15] = row;
    p[0x24] = 0x68;
    p[0x25] = row;
    p[0x35] = row;
    PutFloat(p + 0x3C, bottom);
    p[0x44] = 0x68;
    p[0x45] = row;
    BH_CALL(Gpu_SetPolyFT4)(p);
    BH_CALL(Gfx_CommitPrim)(1, 0x48);
}

// ============================================================================
// Fight 12 (BOSS012, Amalgam, area 28 row 7)
// ============================================================================

// original 0x4398E0: Boss_SetupTable[12]: End = Boss12_End, Exit = BareRet,
// Event = BareRetZero.
extern "C" void __cdecl Boss12_Setup(void) { StoreHooks(bof3::addr::Boss12_End, bof3::addr::BareRet, bof3::addr::BareRetZero); }

// original 0x439900: won (0x904AE8 bit 1, the byte read once): 0x903848 =
// 0x23, 0x904AE8 = the byte | 8, a tail jump to 0x446DE0; else to 0x446E00.
extern "C" void __cdecl Boss12_End(void) {
    const unsigned char end = B(at::kBattleEnd);
    if (end & 2) {
        B(at::kMoveCounter) = 0x23;
        B(at::kBattleEnd) = static_cast<unsigned char>(end | 8);
        BH_AT(Handler, at::kEndWin)();
        return;
    }
    BH_AT(Handler, at::kEndOther)();
}

// ============================================================================
// Kinds 13 and 14 (Balio, Sunder, areas 11 and 41) - fights 13 and 16
// ============================================================================

namespace {
// Kinds 13 and 14's state 0: the tables, +1 = 2; in fight 13 (0x904AAA 0xD)
// 0x939AD8's words +0xD0, +0xB0 and +0xA4 (HP) = 0xFFFF; then
// Sprite_SetAnimationBank(bank), Sprite_SetAnimation(0), and +0x2A = 1 on
// Sprite_Current read after the calls.
void BalioSunderEnter(U fc, U f4, U f8, unsigned short bank) {
    StoreKindTables(fc, f4, f8);
    Sprite_Current[1] = 2;
    if (B(at::kFight) == 0xD) {
        SetWord(Enemy() + 0xD0, 0xFFFF);
        SetWord(Enemy() + 0xB0, 0xFFFF);
        SetWord(Enemy() + 0xA4, 0xFFFF);
    }
    BH_CALL(Sprite_SetAnimationBank)(bank);
    BH_CALL(Sprite_SetAnimation)(0);
    Sprite_Current[0x2A] = 1;
}
// Kinds 13 and 14's hit hooks' floor (Balio's and Sunder's, fights 13 and 16):
// 0x939AD8's HP +0xA4 at 0 made 1.
void HpFloor() {
    unsigned char* const e = Enemy();
    if (Word(e + 0xA4) == 0) SetWord(e + 0xA4, 1);
}
}  // namespace

// original 0x439B10: BossKind_Table[13]: by +1 through BossBalio_Steps (12).
extern "C" unsigned long __cdecl BossBalio_Dispatch(unsigned long through) {
    return Dispatch("BossBalio_Dispatch", 0x64CCD0, 12, 1, through);
}

// original 0x439B30: state 0 (BalioSunderEnter above): BossBalio_Anims,
// BossBalio_Hook, BossBalio_F8, bank 0xEE.
extern "C" void __cdecl BossBalio_Enter(void) { BalioSunderEnter(0x64CC88, bof3::addr::BossBalio_Hook, 0x64CCB8, 0xEE); }

// original 0x439BC0: the +0xF4 hook: by the word's low byte through
// BossBalio_Hooks (3: BossBalio_HookAct, BossBalio_HookHit, BareRet).
extern "C" unsigned long __cdecl BossBalio_Hook(unsigned long word) { return HookDispatch("BossBalio_Hook", 0x64CD00, word); }

// original 0x439BD0: hook entry 0 (the action pick, 0x435BF5): in fight 16
// (0x904AAA 0x10), by the fight's bits 0x904AAD (read once): bit 0 without
// bit 1 - action 0, line 0x26, bit 1 set (the byte read again after the
// call); then (on that byte) bit 4 without bit 5 - action 3, line 0x29, bit 5
// set. The word is not read.
extern "C" void __cdecl BossBalio_HookAct(unsigned) {
    if (B(at::kFight) != 0x10) return;
    unsigned char bits = B(at::kFightFlags);
    if ((bits & 1) && !(bits & 2)) {
        Line(0, 0x26);
        bits = static_cast<unsigned char>(B(at::kFightFlags) | 2);
        B(at::kFightFlags) = bits;
    }
    if ((bits & 0x10) && !(bits & 0x20)) {
        Line(3, 0x29);
        B(at::kFightFlags) = static_cast<unsigned char>(B(at::kFightFlags) | 0x20);
    }
}

// original 0x439C40: hook entry 1 (the hit, 0x4367EE): HP 0 made 1.
extern "C" void __cdecl BossBalio_HookHit(unsigned) { HpFloor(); }

// original 0x439C60: BossKind_Table[14]: by +1 through BossSunder_Steps (12).
extern "C" unsigned long __cdecl BossSunder_Dispatch(unsigned long through) {
    return Dispatch("BossSunder_Dispatch", 0x64CD0C, 12, 1, through);
}

// original 0x439C80: state 0: BossSunder_Anims, BossSunder_Hook,
// BossSunder_F8, bank 0xEF.
extern "C" void __cdecl BossSunder_Enter(void) { BalioSunderEnter(0x64CC94, bof3::addr::BossSunder_Hook, 0x64CCC0, 0xEF); }

// original 0x439D10: the +0xF4 hook: through BossSunder_Hooks (3:
// BossSunder_HookAct, BossSunder_HookHit, BareRet).
extern "C" unsigned long __cdecl BossSunder_Hook(unsigned long word) { return HookDispatch("BossSunder_Hook", 0x64CD3C, word); }

// original 0x439D20: hook entry 0: in fight 16 with the fight's bit 0 (the
// byte read once): without bit 2 - action 0, line 0x25, bit 2 set (read
// again after the call); then bit 3 without bit 4 - action 0, line 0x28, bit
// 4 set (likewise); then with bit 5 - action 3, line 0x2A, no bit set.
extern "C" void __cdecl BossSunder_HookAct(unsigned) {
    if (B(at::kFight) != 0x10) return;
    unsigned char bits = B(at::kFightFlags);
    if (!(bits & 1)) return;
    if (!(bits & 4)) {
        Line(0, 0x25);
        bits = static_cast<unsigned char>(B(at::kFightFlags) | 4);
        B(at::kFightFlags) = bits;
    }
    if ((bits & 8) && !(bits & 0x10)) {
        Line(0, 0x28);
        bits = static_cast<unsigned char>(B(at::kFightFlags) | 0x10);
        B(at::kFightFlags) = bits;
    }
    if (bits & 0x20) Line(3, 0x2A);
}

// original 0x439DB0: hook entry 1: in fight 16 with the acting actor 8,
// 0x939AD8's word +0x108 at 0 made 1; then HP 0 made 1.
extern "C" void __cdecl BossSunder_HookHit(unsigned) {
    if (B(at::kFight) == 0x10 && B(at::kActor) == 8) {
        unsigned char* const e = Enemy();
        if (Word(e + 0x108) == 0) SetWord(e + 0x108, 1);
    }
    HpFloor();
}

// ============================================================================
// Kind 17 (Nina, area 41) - fights 13 and 16
// ============================================================================

// original 0x439E00: BossKind_Table[17]: by +1 through BossNina_Steps (12).
extern "C" unsigned long __cdecl BossNina_Dispatch(unsigned long through) {
    return Dispatch("BossNina_Dispatch", 0x64CD48, 12, 1, through);
}

// original 0x439E20: state 0: the tables (BossNina_Anims, BossNina_Hook,
// BossNina_F8), +8 = 1, +1 = 2.
extern "C" void __cdecl BossNina_Enter(void) {
    StoreKindTables(0x64CCA0, bof3::addr::BossNina_Hook, 0x64CCC8);
    Sprite_Current[8] = 1;
    Sprite_Current[1] = 2;
}

// original 0x439E70: BossNina_Steps 9: by +2 through BossNina_WalkSteps (3:
// BossNina_WalkStart, BossNina_WalkStep, 0x4373C0).
extern "C" unsigned long __cdecl BossNina_WalkDispatch(unsigned long through) {
    return Dispatch("BossNina_WalkDispatch", 0x64CD78, 3, 2, through);
}

// original 0x439E90: Sprite_SetAnimationBank(0xF9); +0x24 &= 0xFE;
// Sprite_SetAnimation(2); +2 up by one - Sprite_Current read after each call.
extern "C" void __cdecl BossNina_WalkStart(void) {
    BH_CALL(Sprite_SetAnimationBank)(0xF9);
    Sprite_Current[0x24] &= 0xFE;
    BH_CALL(Sprite_SetAnimation)(2);
    Sprite_Current[2] += 1;
}

// original 0x439EC0: with +0 bit 7, +2 up by one (eax the sprite); else the
// dword +0x34 up by 0x8000 and Sprite_ScriptTick twice, the second's answer
// the answer.
extern "C" unsigned __cdecl BossNina_WalkStep(void) {
    unsigned char* const s = Sprite_Current;
    if (s[0] & 0x80) {
        s[2] += 1;
        return static_cast<U>(reinterpret_cast<std::uintptr_t>(s));
    }
    SetLong(s + 0x34, static_cast<std::int32_t>(L(s + 0x34) + 0x8000));
    BH_CALL(Sprite_ScriptTick)();
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x439EE0: the +0xF4 hook: through BossNina_Hooks (3:
// BossNina_HookAct, BossNina_HookHit, BareRet).
extern "C" unsigned long __cdecl BossNina_Hook(unsigned long word) { return HookDispatch("BossNina_Hook", 0x64CD84, word); }

// original 0x439EF0: hook entry 0: with the fight's bit 0, action 3; else
// while the wait count 0x675F00 is not 0, action 0 and the count down; at 0,
// action 1, the target 4, 0x904AE2 down by one and the fight's bit 0 set.
extern "C" void __cdecl BossNina_HookAct(unsigned) {
    if (B(at::kFightFlags) & 1) {
        B(at::kAction) = 3;
        return;
    }
    const unsigned char wait = B(at::kWaitTurns);
    if (wait) {
        B(at::kAction) = 0;
        B(at::kWaitTurns) = static_cast<unsigned char>(wait - 1);
        return;
    }
    const unsigned char count = B(at::kCount4AE2);
    B(at::kAction) = 1;
    B(at::kTarget) = 4;
    B(at::kCount4AE2) = static_cast<unsigned char>(count - 1);
    B(at::kFightFlags) = static_cast<unsigned char>(B(at::kFightFlags) | 1);
}

// original 0x439F50: hook entry 1: the name banner (NameBanner above); the
// wait count 0; Field_MemberSprite(1, 1); Sprite_Current +0x29 = 4;
// 0x939AD8's +0xFC = BossNina_Anims2; Sprite_SetAnimation(+8 + 4); the
// fight's bit 6 set.
extern "C" void __cdecl BossNina_HookHit(unsigned) {
    NameBanner();
    B(at::kWaitTurns) = 0;
    BH_CALL(Field_MemberSprite)(1, 1);
    Sprite_Current[0x29] = 4;
    SetLong(Enemy() + 0xFC, 0x64CCAC);
    BH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(Sprite_Current[8] + 4));
    B(at::kFightFlags) = static_cast<unsigned char>(B(at::kFightFlags) | 0x40);
}

// ============================================================================
// Fight 16 (BOSS013, area 11 or 41 row 7: kinds 13, 14, 17)
// ============================================================================

// original 0x43A030: Boss_SetupTable[16]: End = Boss16_End, Exit = Boss16_Exit,
// Event = Boss16_Event; the wait count 0x675F00 = (Rand() & 2) + 8.
extern "C" void __cdecl Boss16_Setup(void) {
    StoreHooks(bof3::addr::Boss16_End, bof3::addr::Boss16_Exit, bof3::addr::Boss16_Event);
    B(at::kWaitTurns) = static_cast<unsigned char>((BH_CALL(Rand)() & 2) + 8);
}

// original 0x43A060: the event hook, al 0 on every path, by the phase code's
// low byte - 2 (BattleRoundEnd_NextRound): unless the fight's bit 6 or a
// wait count left, the name banner, the wait count 0, Sprite_Current and
// 0x939AD8 = enemy 2, Field_MemberSprite(1, 1), 0x939AD8's +0xFC =
// BossNina_Anims2, +0x29 = 4, Sprite_SetAnimation(+8 + 4), bit 6 set. 1: with
// the acting actor 0, the target's bit 6 and not the fight's bit 0, action 0
// and line 0x27. 0: with enemy 0's or 1's dword +0x92 bit 0x2000, the fight's
// bit 3 set.
extern "C" unsigned char __cdecl Boss16_Event(unsigned code) {
    switch (code & 0xFF) {
    case 0:
        if ((L(At(at::kEnemy0 + at::kEnemyStatus)) & 0x2000) || (L(At(at::kEnemy1 + at::kEnemyStatus)) & 0x2000))
            B(at::kFightFlags) |= 8;
        return 0;
    case 1:
        if (B(at::kActor) != 0 || !(B(at::kTarget) & 0x40) || (B(at::kFightFlags) & 1)) return 0;
        Line(0, 0x27);
        return 0;
    case 2:
        if (B(at::kFightFlags) & 0x40) return 0;
        if (B(at::kWaitTurns) != 0) return 0;
        NameBanner();
        B(at::kWaitTurns) = 0;
        Sprite_Current = At(at::kEnemy2);
        SetLong(At(kCurrentEnemy), static_cast<std::int32_t>(at::kEnemy2));
        BH_CALL(Field_MemberSprite)(1, 1);
        SetLong(Enemy() + 0xFC, 0x64CCAC);
        Sprite_Current[0x29] = 4;
        BH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(Sprite_Current[8] + 4));
        B(at::kFightFlags) = static_cast<unsigned char>(B(at::kFightFlags) | 0x40);
        return 0;
    default:
        return 0;
    }
}

// original 0x43A190: the end hook. Won: 0x904AEC = enemy 1's word +0x96 +
// enemy 0's, 0x903848 = 0x14, 0x446DE0; else, with the fight's bit 6,
// Sprite_Current = enemy 2, Sprite_SetAnimationBank(0xF9), +0x24 &= 0xFE,
// +0x2A = 1, Sprite_SetAnimation(1); 0x903848 = 0x50, 0x446E20. Both ways
// then the word 0x9039A2 &= 0xFF7F and 0x904131 = 0x31.
extern "C" void __cdecl Boss16_End(void) {
    if (B(at::kBattleEnd) & 2) {
        const U sum = static_cast<U>(Word(At(at::kEnemy1 + at::kEnemyExp))) + Word(At(at::kEnemy0 + at::kEnemyExp));
        B(at::kMoveCounter) = 0x14;
        SetLong(At(at::kBattleExp), static_cast<std::int32_t>(sum));
        BH_AT(Handler, at::kEndWin)();
    } else {
        if (B(at::kFightFlags) & 0x40) {
            Sprite_Current = At(at::kEnemy2);
            BH_CALL(Sprite_SetAnimationBank)(0xF9);
            Sprite_Current[0x24] &= 0xFE;
            Sprite_Current[0x2A] = 1;
            BH_CALL(Sprite_SetAnimation)(1);
        }
        B(at::kMoveCounter) = 0x50;
        BH_AT(Handler, at::kEndThird)();
    }
    SetWord(At(at::kSaveWord), Word(At(at::kSaveWord)) & 0xFF7F);
    B(at::kFieldByte131) = 0x31;
}

// original 0x43A230: the exit hook. Unless won: (without the fight's bit 0)
// actor 2 - BossActor_ClearBit40, Sprite_Current = BossActor_Find,
// Sprite_SetAnimationBank(0xF9), +0x24 &= 0xFE, then by the fight's bit 6
// (read after that call) Sprite_SetAnimation(1) or Sprite_SetAnimationAt(5,
// 4), +0x2A = 1; actor 1 - bank 0xEF, animation 0, enemy 1's +0x58 / +0x5A;
// actor 0 - bank 0xEE, animation 0, enemy 0's. Both ways then
// Scenario_CallA(1), member 1's +0 |= 0x40, and 0x92BF18 = 6, or 7 when
// 0x903848 (read after the call) is not 0x14.
extern "C" void __cdecl Boss16_Exit(void) {
    if (!(B(at::kBattleEnd) & 2)) {
        if (!(B(at::kFightFlags) & 1)) {
            BH_CALL(BossActor_ClearBit40)(2);
            Sprite_Current = Actor("Boss16_Exit", 2);
            BH_CALL(Sprite_SetAnimationBank)(0xF9);
            Sprite_Current[0x24] &= 0xFE;
            if (B(at::kFightFlags) & 0x40)
                BH_CALL(Sprite_SetAnimation)(1);
            else
                BH_CALL(Sprite_SetAnimationAt)(5, 4);
            Sprite_Current[0x2A] = 1;
        }
        BH_CALL(BossActor_ClearBit40)(1);
        Sprite_Current = Actor("Boss16_Exit", 1);
        BH_CALL(Sprite_SetAnimationBank)(0xEF);
        BH_CALL(Sprite_SetAnimation)(0);
        SetWord(Sprite_Current + 0x58, Word(At(at::kEnemy1 + at::kEnemyPose)));
        SetWord(Sprite_Current + 0x5A, Word(At(at::kEnemy1 + at::kEnemyPose + 2)));
        BH_CALL(BossActor_ClearBit40)(0);
        Sprite_Current = Actor("Boss16_Exit", 0);
        BH_CALL(Sprite_SetAnimationBank)(0xEE);
        BH_CALL(Sprite_SetAnimation)(0);
        SetWord(Sprite_Current + 0x58, Word(At(at::kEnemy0 + at::kEnemyPose)));
        SetWord(Sprite_Current + 0x5A, Word(At(at::kEnemy0 + at::kEnemyPose + 2)));
    }
    BH_CALL(Scenario_CallA)(1);
    B(at::kMember1Flags) |= 0x40;
    B(at::kLeaderPick) = static_cast<unsigned char>(B(at::kMoveCounter) != 0x14 ? 7 : 6);
}

// ============================================================================
// Kinds 15 and 53 (Rocky, area 26; Sample 8, area 164)
// ============================================================================

// original 0x43A360: BossKind_Table[15]: by +1 through BossRocky_Steps (12).
extern "C" unsigned long __cdecl BossRocky_Dispatch(unsigned long through) {
    return Dispatch("BossRocky_Dispatch", 0x64CDAC, 12, 1, through);
}

// original 0x43A380: state 0: the tables (BossRocky_Anims, BossRocky_Hook,
// BossRocky_F8), +1 = 2, a tail jump to Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossRocky_Enter(void) {
    StoreKindTables(0x64CD90, bof3::addr::BossRocky_Hook, 0x64CD9C);
    Sprite_Current[1] = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x43A3C0: the +0xF4 hook: through BossRocky_Hooks (3, all BareRet).
extern "C" unsigned long __cdecl BossRocky_Hook(unsigned long word) { return HookDispatch("BossRocky_Hook", 0x64CDDC, word); }

// original 0x43A3D0: BossKind_Table[53]: by +1 through BossSample8_Steps (12).
extern "C" unsigned long __cdecl BossSample8_Dispatch(unsigned long through) {
    return Dispatch("BossSample8_Dispatch", 0x64CDE8, 12, 1, through);
}

// original 0x43A3F0: state 0: the tables (BossRocky_Anims - Rocky's own -,
// BossSample8_Hook, BossSample8_F8), 0x939AD8's +0x114 |= 8, +1 = 2, a tail
// jump to Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossSample8_Enter(void) {
    StoreKindTables(0x64CD90, bof3::addr::BossSample8_Hook, 0x64CDA4);
    unsigned char* const e = Enemy();
    SetLong(e + 0x114, static_cast<std::int32_t>(L(e + 0x114) | 8));
    Sprite_Current[1] = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x43A450: the +0xF4 hook: through BossSample8_Hooks (3, all BareRet).
extern "C" unsigned long __cdecl BossSample8_Hook(unsigned long word) {
    return HookDispatch("BossSample8_Hook", 0x64CE18, word);
}

// ============================================================================
// Fights 14 and 46 (BOSS014, area 26 row 7; BOSS046)
// ============================================================================

// original 0x43A460: Boss_SetupTable[14]: End = Boss14_End, Exit =
// BossHook_ExitClearActor0, Event = BareRetZero.
extern "C" void __cdecl Boss14_Setup(void) {
    StoreHooks(bof3::addr::Boss14_End, bof3::addr::BossHook_ExitClearActor0, bof3::addr::BareRetZero);
}

// original 0x43A480: won: 0x903848 = 0xC, 0x446DE0; else 0x446E00.
extern "C" void __cdecl Boss14_End(void) { EndByWin(0xC); }

// original 0x43A4B0: Boss_SetupTable[46]: End = BossHook_EndPickWay, Exit =
// BareRet, Event = BareRetZero.
extern "C" void __cdecl Boss46_Setup(void) {
    StoreHooks(bof3::addr::BossHook_EndPickWay, bof3::addr::BareRet, bof3::addr::BareRetZero);
}

// ============================================================================
// Kind 16 (Pooch, area 26) and fight 15 (BOSS015, area 26 row 6)
// ============================================================================

// original 0x43A4D0: BossKind_Table[16]: by +1 through BossPooch_Steps (12).
extern "C" unsigned long __cdecl BossPooch_Dispatch(unsigned long through) {
    return Dispatch("BossPooch_Dispatch", 0x64CE38, 12, 1, through);
}

// original 0x43A4F0: state 0: the tables (BossPooch_Anims, BossPooch_Hook,
// BossPooch_F8), +1 = 2, a tail jump to Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossPooch_Enter(void) {
    StoreKindTables(0x64CE24, bof3::addr::BossPooch_Hook, 0x64CE30);
    Sprite_Current[1] = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x43A530: the +0xF4 hook: through BossPooch_Hooks (3, all BareRet).
extern "C" unsigned long __cdecl BossPooch_Hook(unsigned long word) { return HookDispatch("BossPooch_Hook", 0x64CE68, word); }

// original 0x43A540: Boss_SetupTable[15]: End = Boss15_End, Exit =
// Boss15_Exit, Event = BareRetZero.
extern "C" void __cdecl Boss15_Setup(void) { StoreHooks(bof3::addr::Boss15_End, bof3::addr::Boss15_Exit, bof3::addr::BareRetZero); }

// original 0x43A560: won: 0x903848 = 0x4B, 0x446DE0; else 0x446E00.
extern "C" void __cdecl Boss15_End(void) { EndByWin(0x4B); }

// original 0x43A580: the exit hook: BossActor_Clear(1).
extern "C" void __cdecl Boss15_Exit(void) { BH_CALL(BossActor_Clear)(1); }

// ============================================================================

void BossSc_Inject() {
    if (bof3::WantsShadow("boss_sc")) boss_sc::SelfTest();
    BOF3_INJECT(Boss11_Setup);
    BOF3_INJECT(Boss11_End);
    BOF3_INJECT(Boss11_Exit);
    BOF3_INJECT(BossAmalgam_Dispatch);
    BOF3_INJECT(BossAmalgam_Enter);
    BOF3_INJECT(BossAmalgam_ActDispatch);
    BOF3_INJECT(BossAmalgam_DeathDispatch);
    BOF3_INJECT(BossAmalgam_DeathTick);
    BOF3_INJECT(BossAmalgam_DeathStart);
    BOF3_INJECT(BossAmalgam_DeathMelt);
    BOF3_INJECT(BossAmalgam_DeathEnd);
    BOF3_INJECT(BossAmalgam_Hook);
    BOF3_INJECT(Boss12_Setup);
    BOF3_INJECT(Boss12_End);
    BOF3_INJECT(BossAmalgam_DrawSprite);
    BOF3_INJECT(BossAmalgam_DrawStreak);
    BOF3_INJECT(BossBalio_Dispatch);
    BOF3_INJECT(BossBalio_Enter);
    BOF3_INJECT(BossBalio_Hook);
    BOF3_INJECT(BossBalio_HookAct);
    BOF3_INJECT(BossBalio_HookHit);
    BOF3_INJECT(BossSunder_Dispatch);
    BOF3_INJECT(BossSunder_Enter);
    BOF3_INJECT(BossSunder_Hook);
    BOF3_INJECT(BossSunder_HookAct);
    BOF3_INJECT(BossSunder_HookHit);
    BOF3_INJECT(BossNina_Dispatch);
    BOF3_INJECT(BossNina_Enter);
    BOF3_INJECT(BossNina_WalkDispatch);
    BOF3_INJECT(BossNina_WalkStart);
    BOF3_INJECT(BossNina_WalkStep);
    BOF3_INJECT(BossNina_Hook);
    BOF3_INJECT(BossNina_HookAct);
    BOF3_INJECT(BossNina_HookHit);
    BOF3_INJECT(Boss16_Setup);
    BOF3_INJECT(Boss16_Event);
    BOF3_INJECT(Boss16_End);
    BOF3_INJECT(Boss16_Exit);
    BOF3_INJECT(BossRocky_Dispatch);
    BOF3_INJECT(BossRocky_Enter);
    BOF3_INJECT(BossRocky_Hook);
    BOF3_INJECT(BossSample8_Dispatch);
    BOF3_INJECT(BossSample8_Enter);
    BOF3_INJECT(BossSample8_Hook);
    BOF3_INJECT(Boss14_Setup);
    BOF3_INJECT(Boss14_End);
    BOF3_INJECT(Boss46_Setup);
    BOF3_INJECT(BossPooch_Dispatch);
    BOF3_INJECT(BossPooch_Enter);
    BOF3_INJECT(BossPooch_Hook);
    BOF3_INJECT(Boss15_Setup);
    BOF3_INJECT(Boss15_End);
    BOF3_INJECT(Boss15_Exit);
}

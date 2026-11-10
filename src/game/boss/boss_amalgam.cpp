// Moved here by tools/move_funcs.py from boss_sc.cpp (docs/source-layout-round.md).
#include "game/boss/boss_amalgam.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sc_callees.h"
#include "game/cheats.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_sc::at;

}  // namespace

namespace {

using U = std::uint32_t;

using boss_harness::Handler;

using move_script::At;

using move_script::Long;

using move_script::Word;

unsigned char& B(U address) { return move_script::At(address)[0]; }

unsigned char* Enemy() { return reinterpret_cast<unsigned char*>(BattleE3_CurrentEnemy.Native()); }

// Sprite_Current (rv::Current<T>()): the boss enemy in its kind's handlers,
// the slot in an effect task.
unsigned char* Packet() { return Gfx_PacketNext; }

U L(const unsigned char* p) { return static_cast<U>(Long(p)); }

void PutFloatBossSc(unsigned char* p, U v) {
    const float f = static_cast<float>(static_cast<std::int32_t>(v));   // fild / fstp: nearest
    std::memcpy(p, &f, sizeof f);
}

// The three hooks a set-up stores (BattleHook_End / _Exit / _Event).
void StoreHooks(U end, U exit, U event) {
    BattleHook_End = static_cast<std::int32_t>(end);
    BattleHook_Exit = static_cast<std::int32_t>(exit);
    BattleHook_Event = static_cast<std::int32_t>(event);
}

// A kind's entrance: 0x939AD8's +0xFC (its animation bytes), +0xF4 (its hook)
// and +0xF8, in that order, 0x939AD8 read before each (no call between).
void StoreKindTables(U fc, U f4, U f8) {
    rv::As<records::BattleEnemy>(Enemy())->anim_table = static_cast<std::int32_t>(fc);
    rv::As<records::BattleEnemy>(Enemy())->event_hook = static_cast<std::int32_t>(f4);
    rv::As<records::BattleEnemy>(Enemy())->cue_table = static_cast<std::int32_t>(f8);
}

using EntryBossSc = U (__cdecl*)(U);

EntryBossSc EntryAt(U table, unsigned index) { return reinterpret_cast<EntryBossSc>(static_cast<std::uintptr_t>(L(At(table + 4 * index)))); }

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, a
// Fatal past them (the original jumps through whatever follows). The jmp
// leaves the stack as the dispatcher found it, so the entry sees the word
// above the return address that the dispatcher saw: ours hands it on (an
// entry that reads it - Port_DroppedCall, a bare ret with a byte - sees what
// it sees under the original), and answers the entry's eax.
U DispatchBossSc(const char* who, U table, unsigned entries, unsigned at, U through) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sc.md section 7)",
                    who, at, state, entries, (unsigned)table);
    return EntryAt(table, state)(through);   // the entry as read: the fuzz swaps the table's cells for its recorders
}

// An enemy's +0xF4 hook table: jmp [table + 4 * (word & 0xFF)], 3 entries.
U HookDispatchBossSc(const char* who, U table, U word) {
    const unsigned index = word & 0xFF;
    if (index >= 3)
        bof3::Fatal("%s: the hook's word is 0x%X, past the 3 entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sc.md section 7)",
                    who, (unsigned)word, (unsigned)table);
    return EntryAt(table, index)(word);
}

}  // namespace

// original 0x4396B0: BossKind_Table[12]: by +1 through BossAmalgam_Steps (12).
extern "C" unsigned long __cdecl BossAmalgam_Dispatch(unsigned long through) {
    return DispatchBossSc("BossAmalgam_Dispatch", ::bof3::Img(0x64CC24), 12, 1, through);
}

// original 0x4396D0: state 0: the kind's tables (+0xFC BossAmalgam_Anims,
// +0xF4 BossAmalgam_Hook, +0xF8 BossAmalgam_F8), 0x455290(Sprite_Current,
// BossAmalgam_SlotScripts) - a field slot script for the sprite -, +1 = 2 on
// Sprite_Current read after the call, and a tail jump to Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossAmalgam_Enter(void) {
    StoreKindTables(::bof3::Img(0x64CBC4), ::bof3::Code(bof3::addr::BossAmalgam_Hook), ::bof3::Img(0x64CBD0));
    BH_AT(void (__cdecl*)(unsigned char*, U), at::kSlotStart.v)(Sprite_Current, ::bof3::Img(0x64CC14));
    rv::Current<records::BattleEnemy>()->state = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x439730: BossAmalgam_Steps 6: by +2 through BossAmalgam_ActSubs
// (6: EnemyOp_ActBegin, EnemyOp_HitDispatch, EnemyOp_ActBegin, EnemyOp_Act3Dispatch,
// BossAmalgam_DeathDispatch, EnemyOp_Act5Dispatch - EnemyOp_ActSubs with the death at 4).
extern "C" unsigned long __cdecl BossAmalgam_ActDispatch(unsigned long through) {
    return DispatchBossSc("BossAmalgam_ActDispatch", ::bof3::Img(0x64CC54), 6, 2, through);
}

// original 0x439750: BossAmalgam_ActSubs 4: by +3 through BossAmalgam_DeathSteps
// (4: Tick, Start, Melt, End).
extern "C" unsigned long __cdecl BossAmalgam_DeathDispatch(unsigned long through) {
    return DispatchBossSc("BossAmalgam_DeathDispatch", ::bof3::Img(0x64CC6C), 4, 3, through);
}

// original 0x439770: death step 0: Sprite_ScriptTick; when it answers al not
// 0, +3 = 1 (Sprite_Current read after the call) and eax is that pointer;
// else eax is the tick's answer.
extern "C" unsigned __cdecl BossAmalgam_DeathTick(void) {
    const unsigned char done = BH_CALL(Sprite_ScriptTick)();
    if (done == 0) return done;
    unsigned char* const s = Sprite_Current;
    auto* const cur_enemy = rv::As<records::BattleEnemy>(s);
    cur_enemy->state3 = 1;
    return Low32(s);
}

// original 0x439790: death step 1: 0x904AA9 |= 4; Sprite_ScriptTickOnce (not
// read); Gfx_ClearRect(0x340, 0x100, 0x80, 0x100); Sound_PlayById(0x601); then
// on Sprite_Current: +0x18 / +0x1C = the words +0x2E / +0x30 sign-extended,
// +0x20 = 0, +0x2E = 0x32, +0x30 = 0x41, +0x24 |= 0x88;
// BossAmalgam_DrawSprite(+0x20's word); +3 = 2 (read after the call).
extern "C" void __cdecl BossAmalgam_DeathStart(void) {
    B(::bof3::Img(at::kRoundFlagsHi.v)) |= 4;
    BH_CALL(Sprite_ScriptTickOnce)();
    BH_CALL(Gfx_ClearRect)(0x340, 0x100, 0x80, 0x100);
    BH_CALL(Sound_PlayById)(0x601);
    unsigned char* const s = Sprite_Current;
    auto* const cur_enemy = rv::As<records::BattleEnemy>(s);
    rv::Store<std::int32_t>(cur_enemy->work_18, static_cast<std::int32_t>(cur_enemy->screen_x));
    rv::Store<std::int32_t>(cur_enemy->work_1c, static_cast<std::int32_t>(cur_enemy->screen_y));
    rv::Store<std::int32_t>(cur_enemy->work_20, 0);
    cur_enemy->screen_x = 0x32;
    cur_enemy->screen_y = 0x41;
    s[0x24] |= 0x88;
    BH_CALL(BossAmalgam_DrawSprite)(Word(s + 0x20));
    rv::Current<records::BattleEnemy>()->state3 = 2;
}

// original 0x439830: death step 2: BossAmalgam_DrawSprite(+0x20's word),
// BossAmalgam_DrawStreak(+0x20's word, Sprite_Current read again), then the
// dword +0x20 up by 2 and at 0x56 +3 = 3.
extern "C" void __cdecl BossAmalgam_DeathMelt(void) {
    BH_CALL(BossAmalgam_DrawSprite)(Word(Sprite_Current + 0x20));
    BH_CALL(BossAmalgam_DrawStreak)(Word(Sprite_Current + 0x20));
    unsigned char* const s = Sprite_Current;
    auto* const cur_enemy = rv::As<records::BattleEnemy>(s);
    rv::Store<std::int32_t>(cur_enemy->work_20, static_cast<std::int32_t>(static_cast<U>(cur_enemy->work_20) + 2));
    if (static_cast<U>(cur_enemy->work_20) == 0x56) cur_enemy->state3 = 3;
}

// original 0x439870: death step 3: 0x454A80(Sprite_Current) - its field slots
// released -, Battle_EnemyDefeated, 0x939AD8's +0x110 |= 0x1000, then
// Sprite_Current +0 &= 0xBF, +1 = 3, +2 = 0, +3 = 0.
extern "C" void __cdecl BossAmalgam_DeathEnd(void) {
    BH_AT(void (__cdecl*)(unsigned char*), at::kSlotsReleaseFor.v)(Sprite_Current);
    BH_CALL(Battle_EnemyDefeated)();
    auto* const enemy_rec = rv::As<records::BattleEnemy>(Enemy());
    enemy_rec->flags_110 = static_cast<std::int32_t>(static_cast<U>(enemy_rec->flags_110) | 0x1000);
    auto* const cur_enemy = rv::Current<records::BattleEnemy>();
    cur_enemy->flags &= 0xBF;
    cur_enemy->state = 3;
    cur_enemy->step = 0;
    cur_enemy->state3 = 0;
}

// original 0x4398D0: the +0xF4 hook: by the word's low byte through
// BossAmalgam_Hooks (3, all BareRet).
extern "C" unsigned long __cdecl BossAmalgam_Hook(unsigned long word) {
    return HookDispatchBossSc("BossAmalgam_Hook", ::bof3::Img(0x64CC7C), word);
}

// original 0x4398E0: Boss_SetupTable[12]: End = Boss12_End, Exit = BareRet,
// Event = BareRetZero.
extern "C" void __cdecl Boss12_Setup(void) { StoreHooks(::bof3::Code(bof3::addr::Boss12_End), ::bof3::Code(bof3::addr::BareRet), ::bof3::Code(bof3::addr::BareRetZero)); }

// original 0x439900: won (0x904AE8 bit 1, the byte read once): 0x903848 =
// 0x23, 0x904AE8 = the byte | 8, a tail jump to 0x446DE0; else to 0x446E00.
extern "C" void __cdecl Boss12_End(void) {
    const unsigned char end = Battle_EndFlags;
    if (end & 2) {
        B(::bof3::Img(at::kMoveCounter.v)) = 0x23;
        Battle_EndFlags = static_cast<unsigned char>(end | 8);
        BH_AT(Handler, at::kEndWin.v)();
        return;
    }
    BH_AT(Handler, at::kEndOther.v)();
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
    PutFloatBossSc(p + 8, static_cast<U>(rv::As<records::BattleEnemy>(s)->work_18) - 0x28);
    PutFloatBossSc(p + 0xC, static_cast<U>(rv::As<records::BattleEnemy>(s)->work_20) + static_cast<U>(rv::As<records::BattleEnemy>(s)->work_1c) - 0x3C);
    p[0x14] = 0;
    rv::Store<unsigned>(rv::As<records::GpuSprt>(p)->w, 0x68);  // a SPRT: Gpu_SetSprt below
    p[0x15] = static_cast<unsigned char>(v);
    rv::Store<unsigned>(rv::As<records::GpuSprt>(p)->h, 0x56);
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
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, tpage);  // a POLY_FT4: Gpu_SetPolyFT4 below
    p[4] = p[5] = p[6] = 0x80;
    const unsigned char* const s = Sprite_Current;
    const U left = static_cast<U>(rv::As<records::BattleEnemy>(s)->work_18) - 0x28, right = static_cast<U>(rv::As<records::BattleEnemy>(s)->work_18) + 0x40, bottom = static_cast<U>(rv::As<records::BattleEnemy>(s)->work_20) + static_cast<U>(rv::As<records::BattleEnemy>(s)->work_1c) - 0x3C;
    rv::Store<float>(rv::As<records::GpuPolyFT4>(p)->y0, 0.0f);
    PutFloatBossSc(p + 8, left);
    rv::Store<float>(rv::As<records::GpuPolyFT4>(p)->y1, 0.0f);
    PutFloatBossSc(p + 0x18, right);
    PutFloatBossSc(p + 0x28, left);
    PutFloatBossSc(p + 0x2C, bottom);
    PutFloatBossSc(p + 0x38, right);
    const auto row = static_cast<unsigned char>(v);
    p[0x14] = 0;
    p[0x34] = 0;
    p[0x15] = row;
    p[0x24] = 0x68;
    p[0x25] = row;
    p[0x35] = row;
    PutFloatBossSc(p + 0x3C, bottom);
    p[0x44] = 0x68;
    p[0x45] = row;
    BH_CALL(Gpu_SetPolyFT4)(p);
    BH_CALL(Gfx_CommitPrim)(1, 0x48);
}

void BossAmalgam_Inject() {
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
}

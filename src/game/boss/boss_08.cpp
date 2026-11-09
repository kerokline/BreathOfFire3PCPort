// Moved here by tools/move_funcs.py from boss_h.cpp, boss_sb.cpp, boss_sc.cpp (docs/source-layout-round.md).
#include "game/boss/boss_08.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/boss/boss_08_callees.h"
#include "game/boss_h_callees.h"
#include "game/boss_harness.h"
#include "game/boss_sb_callees.h"
#include "game/boss_sc_callees.h"
#include "game/cheats.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_08::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

unsigned char* Enemy() { return reinterpret_cast<unsigned char*>(BattleE3_CurrentEnemy.Native()); }

// Sprite_Current (rv::Current<T>()): the boss enemy in its kind's handlers,
// the slot in an effect task.
void PutFloat(unsigned char* p, std::int32_t v) {
    const float f = static_cast<float>(v);   // fild / fstp: nearest, as cvtsi2ss
    std::memcpy(p, &f, sizeof f);
}

// A named .data table's address (symbols.gen.h binds the name to a typed pointer).
template <typename T> U AddressOf(T* p) { return Low32(p); }

// Kinds 8..11's colours: three bytes a kind from BossTorast_Colours (kind 8's
// at its start); the original indexes by the kind unchecked, so another kind
// reads the .data around it.
const unsigned char* KindColour(unsigned kind) { return At(AddressOf(BossTorast_Colours) + 3 * (kind - 8)); }

using boss_harness::Handler;

using move_script::SetWord;

unsigned char& B(U address) { return move_script::At(address)[0]; }

void Setup(U end, U exit, U event) {
    BattleHook_End = static_cast<std::int32_t>(end);
    BattleHook_Exit = static_cast<std::int32_t>(exit);
    BattleHook_Event = static_cast<std::int32_t>(event);
}

// BossActor_Find(tag) into Sprite_Current. The original writes through the
// answer untested; with no field actor carrying the tag it writes near
// address 0 and faults. Ours aborts there with a message (the owner's rule,
// round9 doc section 6; docs/boss_sb.md section 6), as BH's spawn writers do.
void FindActor(const char* who, unsigned tag) {
    unsigned char* const actor = BH_CALL(BossActor_Find)(tag);
    if (actor == nullptr)
        bof3::Fatal("%s: no field actor carries tag %u - the original writes through the null answer (docs/boss_sb.md section 6)",
                    who, tag);
    Sprite_Current = actor;
}

// The exit hook of set-ups 8, 9, 10: actor `tag` shown, found, bank 0x83,
// (the flip 1 in 9 and 10), animation 0, enemy 0's words +0x58 / +0x5A.
void ExitPoseOne(unsigned tag, bool flip) {
    BH_CALL(BossActor_ClearBit40)(tag);
    FindActor("ExitPoseOne", tag);
    BH_CALL(Sprite_SetAnimationBank)(0x83);
    if (flip) Sprite_Current[0x2A] = 1;
    BH_CALL(Sprite_SetAnimation)(0);
    SetWord(Sprite_Current + 0x58, BossSb_Enemy0Pose);
    SetWord(Sprite_Current + 0x5A, Battle_Enemies[0].frame_word);
}

// The end hook of set-ups 8, 9, 10: the win (0x904AE8 bit 1) - the movement
// script's variable 3 = `var`, 0x446DE0; otherwise 0x446E00.
void EndVar(unsigned char var) {
    if (Battle_EndFlags & 2) {
        B(::bof3::Img(at::kScriptVar3.v)) = var;
        BH_AT(Handler, boss_sb::at::kEndWin.v)();
        return;
    }
    BH_AT(Handler, boss_sb::at::kEndOther.v)();
}

// The three hooks a set-up stores (BattleHook_End / _Exit / _Event).
void StoreHooks(U end, U exit, U event) {
    BattleHook_End = static_cast<std::int32_t>(end);
    BattleHook_Exit = static_cast<std::int32_t>(exit);
    BattleHook_Event = static_cast<std::int32_t>(event);
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

// The end hooks' common shape: on the win (0x904AE8 bit 1) the move-script
// counter byte 0x903848 = scene and the end phase's step 1, else step 2.
void EndByWin(unsigned char scene) {
    if (Battle_EndFlags & 2) {
        B(::bof3::Img(at::kMoveCounter.v)) = scene;
        BH_AT(Handler, boss_sc::at::kEndWin.v)();
        return;
    }
    BH_AT(Handler, boss_sc::at::kEndOther.v)();
}

}  // namespace

// original 0x439240: Boss_SetupTable[8]: Event BareRetZero.
extern "C" void __cdecl Boss08_Setup(void) { Setup(::bof3::Code(bof3::addr::Boss08_End), ::bof3::Code(bof3::addr::Boss08_Exit), ::bof3::Code(bof3::addr::BareRetZero)); }

// original 0x439260: set-up 8's end hook: the win - variable 3 = 0x11, 0x446DE0;
// otherwise 0x446E00.
extern "C" void __cdecl Boss08_End(void) { EndVar(0x11); }

// original 0x439280: set-up 8's exit hook: actor 0 from enemy 0's pose words
// (bank 0x83, animation 0, no flip).
extern "C" void __cdecl Boss08_Exit(void) { ExitPoseOne(0, false); }

// original 0x4392D0: Boss_SetupTable[9].
extern "C" void __cdecl Boss09_Setup(void) { Setup(::bof3::Code(bof3::addr::Boss09_End), ::bof3::Code(bof3::addr::Boss09_Exit), ::bof3::Code(bof3::addr::BareRetZero)); }

// original 0x4392F0: set-up 9's end hook: variable 3 = 0x20 on the win.
extern "C" void __cdecl Boss09_End(void) { EndVar(0x20); }

// original 0x439310: set-up 9's exit hook: actor 1, the flip 1, enemy 0's words.
extern "C" void __cdecl Boss09_Exit(void) { ExitPoseOne(1, true); }

// original 0x439370: Boss_SetupTable[10].
extern "C" void __cdecl Boss10_Setup(void) { Setup(::bof3::Code(bof3::addr::Boss10_End), ::bof3::Code(bof3::addr::Boss10_Exit), ::bof3::Code(bof3::addr::BareRetZero)); }

// original 0x439390: set-up 10's end hook: variable 3 = 0x30 on the win.
extern "C" void __cdecl Boss10_End(void) { EndVar(0x30); }

// original 0x4393B0: set-up 10's exit hook: actor 2, the flip 1, enemy 0's words.
extern "C" void __cdecl Boss10_Exit(void) { ExitPoseOne(2, true); }

// original 0x439410: Boss_SetupTable[11]: BattleHook_End = Boss11_End,
// _Exit = Boss11_Exit, _Event = BareRetZero.
extern "C" void __cdecl Boss11_Setup(void) { StoreHooks(::bof3::Code(bof3::addr::Boss11_End), ::bof3::Code(bof3::addr::Boss11_Exit), ::bof3::Code(bof3::addr::BareRetZero)); }

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
    SetWord(Sprite_Current + 0x58, Battle_Enemies[0].script_pos);
    SetWord(Sprite_Current + 0x5A, Battle_Enemies[0].frame_word);
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
    BH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, tpage & 0xFFFF, 0);
    BH_CALL(Gfx_CommitPrim)(3, 0xC);
    for (unsigned i = 0; i < 64; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        BH_CALL(Gpu_SetPolyG3)(p);
        p[4] = 0xFF;
        p[5] = 0xFF;
        p[6] = 0xFF;
        const unsigned char* const c = KindColour(rv::As<records::BattleEnemy>(Enemy())->run_state);
        p[0x14] = c[0];
        p[0x15] = c[1];
        p[0x16] = c[2];
        p[0x24] = c[0];
        p[0x25] = c[1];
        p[0x26] = c[2];
        auto* const cur_enemy = rv::Current<records::BattleEnemy>();
        const std::int32_t x = static_cast<std::int32_t>(cur_enemy->screen_x), y = static_cast<std::int32_t>(cur_enemy->screen_y);
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

void Boss08_Inject() {
    BOF3_INJECT(Boss08_Setup);
    BOF3_INJECT(Boss08_End);
    BOF3_INJECT(Boss08_Exit);
    BOF3_INJECT(Boss09_Setup);
    BOF3_INJECT(Boss09_End);
    BOF3_INJECT(Boss09_Exit);
    BOF3_INJECT(Boss10_Setup);
    BOF3_INJECT(Boss10_End);
    BOF3_INJECT(Boss10_Exit);
    BOF3_INJECT(Boss11_Setup);
    BOF3_INJECT(Boss11_End);
    BOF3_INJECT(Boss11_Exit);
    BOF3_INJECT(BossTorast_DrawRing);
}

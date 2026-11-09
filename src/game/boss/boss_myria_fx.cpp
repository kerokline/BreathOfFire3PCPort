// Moved here by tools/move_funcs.py from battle_e3.cpp, boss_sj.cpp (docs/source-layout-round.md).
#include "game/boss/boss_myria_fx.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_e3_callees.h"
#include "game/boss/boss_myria_fx_callees.h"
#include "game/boss_harness.h"
#include "game/boss_sj_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_myria_fx::at;

}  // namespace

namespace {

using U = std::uint32_t;

using boss_harness::Handler;

using boss_harness::Phase;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

using RoundHighFn = U (__cdecl*)(U, U);

// Sprite_Current (rv::Current<T>()): the boss enemy in its kind's handlers,
// the slot in an effect task.
unsigned char* Owner() { return At(BattleE2_Owner); }

// A named .data table's address (symbols.gen.h binds the name to a typed pointer).
template <typename T> U AddressOf(T* p) { return Low32(p); }

// An effect task's dispatcher: the same jmp by +1 through a table of the
// task's states, which take no word.
void DispatchTask(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sj.md section 6)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * state)))))();
}

// A stack table: sub esp, 4n; mov [esp + 4i], handler i; call [esp + 4 * +2].
// The handlers are the group's own (or BattleFx_FreeTask), called by their
// addresses; past n the original calls through its caller's frame.
void Steps(const char* who, const U* handlers, unsigned n) {
    const unsigned step = Sprite_Current[2];
    if (step >= n)
        bof3::Fatal("%s: step byte +2 is %u, past its stack table of %u - the original calls through its caller's frame "
                    "(docs/boss_sj.md section 6)",
                    who, step, n);
    Phase(handlers[step])();
}

// The ten-byte pose tables Myria's effect builds on its stack, indexed by the
// word 0x904B7E: 10 and above read the frame's two unset bytes and then its
// caller's stack.
unsigned char Pose(const char* who, const unsigned char (&poses)[10]) {
    const unsigned i = BossSa_EndCount;
    if (i >= 10)
        bof3::Fatal("%s: the pose index 0x904B7E is %u, past its ten-byte stack table - the original reads its own frame "
                    "and its caller's (docs/boss_sj.md section 6)",
                    who, i);
    return poses[i];
}

constexpr ::bof3::image::CodeVa kFollow{bof3::addr::BossMyriaFx_Follow};   // BossMyriaFx_Follow, called directly by the states

// Myria's effect, steps 0 of states 0..2: Sprite_SetAnimationBank(bank); the
// Sprite_Current of after the call +0x48 = 0, +0x24 = 0, the dword +0x3C = 0;
// the state's loop called; +2 up by one (Sprite_Current read after).
void FxEnter(unsigned bank, U loop) {
    BH_CALL(Sprite_SetAnimationBank)(static_cast<unsigned short>(bank));
    Sprite_Current[0x48] = 0;
    Sprite_Current[0x24] = 0;
    SetLong(Sprite_Current + 0x3C, 0);
    Phase(loop)();
    ++Sprite_Current[2];
}

// Steps 1 of states 0..2: with the owner's +0xB set, Sprite_SetAnimation of
// the state's pose; BattleEnemy_ScriptTick, Sprite_QueueOverlay,
// BossMyriaFx_Follow.
void FxLoop(const char* who, const unsigned char (&poses)[10]) {
    if (Owner()[0xB] != 0) BH_CALL(Sprite_SetAnimation)(Pose(who, poses));
    BH_CALL(BattleEnemy_ScriptTick)();
    BH_CALL(Sprite_QueueOverlay)();
    Phase(kFollow.v)();
}

// Steps 0 of states 3..6: bank; +0x48, +0x24, +0x3C = 0; Sprite_SetAnimation
// of the pose (always); the tick (once or not), Sprite_QueueOverlay,
// BossMyriaFx_Follow; +2 up by one (Sprite_Current read after the calls).
void FxEnterPosed(const char* who, unsigned bank, const unsigned char (&poses)[10], bool once) {
    BH_CALL(Sprite_SetAnimationBank)(static_cast<unsigned short>(bank));
    Sprite_Current[0x48] = 0;
    Sprite_Current[0x24] = 0;
    SetLong(Sprite_Current + 0x3C, 0);
    BH_CALL(Sprite_SetAnimation)(Pose(who, poses));
    if (once)
        BH_CALL(BattleEnemy_ScriptTickOnce)();
    else
        BH_CALL(BattleEnemy_ScriptTick)();
    BH_CALL(Sprite_QueueOverlay)();
    Phase(kFollow.v)();
    ++Sprite_Current[2];
}

// Sprite_QueueOverlay, then a tail jump to BossMyriaFx_Follow.
void QueueAndFollow() {
    BH_CALL(Sprite_QueueOverlay)();
    Phase(kFollow.v)();
}

// One axis of BossMyriaFx_Follow: the s16 at BossMyriaFx_Drift[2 * +1 + axis]
// of the Sprite_Current read now, scaled (with +0x48) by the dword `scale_at`
// and handed to 0x441090 with it, or shifted to the high word and handed with
// 0x10000; ax added to the word `word_at` of that same sprite (the original
// takes the word's address before the call).
void Drift(bool scaled, unsigned axis, unsigned scale_at, unsigned word_at) {
    unsigned char* const s = Sprite_Current;
    const unsigned state = s[1];
    if (state >= 7)
        bof3::Fatal("BossMyriaFx_Follow: state byte +1 is %u, past the 7 pairs of BossMyriaFx_Drift - the original reads "
                    "the .data after (docs/boss_sj.md section 6)",
                    state);
    const auto d = static_cast<U>(static_cast<std::int32_t>(BossMyriaFx_Drift[2 * state + axis]));
    unsigned char* const w = s + word_at;
    U answer;
    if (scaled) {
        const U scale = static_cast<U>(Long(s + scale_at));
        answer = BH_AT(RoundHighFn, at::kRoundHigh.v)(d * scale, scale);
    } else {
        answer = BH_AT(RoundHighFn, at::kRoundHigh.v)(d << 16, 0x10000);
    }
    SetWord(w, static_cast<unsigned>(Word(w) + answer) & 0xFFFF);
}

}  // namespace

// original 0x440830: slot 5: 0x939AD8 = enemy 0's object, then jmp
// [BossMyriaFx_States + 4 * Sprite_Current +1] (7: states 0..6).
extern "C" void __cdecl BossMyriaFx_Dispatch(void) {
    BattleE3_CurrentEnemy = Battle_Enemies;
    DispatchTask("BossMyriaFx_Dispatch", AddressOf(BossMyriaFx_States), 7);
}

// State 0: a stack table by +2 of Enter / Loop (0x440850); Enter (0x440880):
// bank 0x30D; Loop (0x4408C0): poses 0, 1, 1, 1, 0, 1, 1, 1, 1, 1.
extern "C" void __cdecl BossMyriaFx_State0(void) {
    static const U kSteps[] = {bof3::addr::BossMyriaFx_State0Enter, bof3::addr::BossMyriaFx_State0Loop};
    Steps("BossMyriaFx_State0", kSteps, 2);
}

extern "C" void __cdecl BossMyriaFx_State0Enter(void) { FxEnter(0x30D, bof3::addr::BossMyriaFx_State0Loop); }

extern "C" void __cdecl BossMyriaFx_State0Loop(void) {
    static const unsigned char kPoses[10] = {0, 1, 1, 1, 0, 1, 1, 1, 1, 1};
    FxLoop("BossMyriaFx_State0Loop", kPoses);
}

// State 1 (0x440930): Enter (0x440960) bank 0x30C; Loop (0x4409A0) poses 1, 3,
// 5, 3, 3, 3, 3, 3, 3, 3.
extern "C" void __cdecl BossMyriaFx_State1(void) {
    static const U kSteps[] = {bof3::addr::BossMyriaFx_State1Enter, bof3::addr::BossMyriaFx_State1Loop};
    Steps("BossMyriaFx_State1", kSteps, 2);
}

extern "C" void __cdecl BossMyriaFx_State1Enter(void) { FxEnter(0x30C, bof3::addr::BossMyriaFx_State1Loop); }

extern "C" void __cdecl BossMyriaFx_State1Loop(void) {
    static const unsigned char kPoses[10] = {1, 3, 5, 3, 3, 3, 3, 3, 3, 3};
    FxLoop("BossMyriaFx_State1Loop", kPoses);
}

// State 2 (0x440A10): Enter (0x440A40) bank 0x30C; Loop (0x440A80) poses 0, 2,
// 4, 2, 2, 2, 2, 2, 2, 2.
extern "C" void __cdecl BossMyriaFx_State2(void) {
    static const U kSteps[] = {bof3::addr::BossMyriaFx_State2Enter, bof3::addr::BossMyriaFx_State2Loop};
    Steps("BossMyriaFx_State2", kSteps, 2);
}

extern "C" void __cdecl BossMyriaFx_State2Enter(void) { FxEnter(0x30C, bof3::addr::BossMyriaFx_State2Loop); }

extern "C" void __cdecl BossMyriaFx_State2Loop(void) {
    static const unsigned char kPoses[10] = {0, 2, 4, 2, 2, 2, 2, 2, 2, 2};
    FxLoop("BossMyriaFx_State2Loop", kPoses);
}

// State 3 (0x440AF0): Enter (0x440B20) bank 0x30D, poses 0, 2, 0, 4, 0, 5, 0,
// 0, 0, 0, the tick once; Wait (0x440BB0): BattleEnemy_ScriptTick, then with
// the owner's +1 at 2 (read after the call) a tail jump to
// BattleTask_FreeCurrent, else Sprite_QueueOverlay and BossMyriaFx_Follow.
extern "C" void __cdecl BossMyriaFx_State3(void) {
    static const U kSteps[] = {bof3::addr::BossMyriaFx_State3Enter, bof3::addr::BossMyriaFx_State3Wait};
    Steps("BossMyriaFx_State3", kSteps, 2);
}

extern "C" void __cdecl BossMyriaFx_State3Enter(void) {
    static const unsigned char kPoses[10] = {0, 2, 0, 4, 0, 5, 0, 0, 0, 0};
    FxEnterPosed("BossMyriaFx_State3Enter", 0x30D, kPoses, true);
}

extern "C" void __cdecl BossMyriaFx_State3Wait(void) {
    BH_CALL(BattleEnemy_ScriptTick)();
    if (Owner()[1] == 2) {
        BH_CALL(BattleTask_FreeCurrent)();
        return;
    }
    QueueAndFollow();
}

// State 4 (0x440BD0): Enter (0x440C00) bank 0x318, poses 0, 0, 5, 0, 0, 0, 0,
// 0, 0, 0, the tick once; Wait (0x440C90): when BattleEnemy_ScriptTickOnce
// answers al not 0, the owner's +1 up by one and +2 = 0 and a tail jump to
// BattleTask_FreeCurrent; else Sprite_QueueOverlay and BossMyriaFx_Follow.
extern "C" void __cdecl BossMyriaFx_State4(void) {
    static const U kSteps[] = {bof3::addr::BossMyriaFx_State4Enter, bof3::addr::BossMyriaFx_State4Wait};
    Steps("BossMyriaFx_State4", kSteps, 2);
}

extern "C" void __cdecl BossMyriaFx_State4Enter(void) {
    static const unsigned char kPoses[10] = {0, 0, 5, 0, 0, 0, 0, 0, 0, 0};
    FxEnterPosed("BossMyriaFx_State4Enter", 0x318, kPoses, true);
}

extern "C" void __cdecl BossMyriaFx_State4Wait(void) {
    if (static_cast<unsigned char>(BH_CALL(BattleEnemy_ScriptTickOnce)()) != 0) {
        unsigned char* const o = Owner();
        ++o[1];
        Owner()[2] = 0;
        BH_CALL(BattleTask_FreeCurrent)();
        return;
    }
    QueueAndFollow();
}

// State 5 (0x440CC0): Enter (0x440CF0) bank 0x318, poses 0, 0, 0, 0, 4, 0, 0,
// 2, 0, 3, the tick each frame; Wait (0x440D80): with the owner's +1 at 2 a
// tail jump to BattleTask_FreeCurrent; else BattleEnemy_ScriptTick,
// Sprite_QueueOverlay, BossMyriaFx_Follow.
extern "C" void __cdecl BossMyriaFx_State5(void) {
    static const U kSteps[] = {bof3::addr::BossMyriaFx_State5Enter, bof3::addr::BossMyriaFx_State5Wait};
    Steps("BossMyriaFx_State5", kSteps, 2);
}

extern "C" void __cdecl BossMyriaFx_State5Enter(void) {
    static const unsigned char kPoses[10] = {0, 0, 0, 0, 4, 0, 0, 2, 0, 3};
    FxEnterPosed("BossMyriaFx_State5Enter", 0x318, kPoses, false);
}

extern "C" void __cdecl BossMyriaFx_State5Wait(void) {
    if (Owner()[1] == 2) {
        BH_CALL(BattleTask_FreeCurrent)();
        return;
    }
    BH_CALL(BattleEnemy_ScriptTick)();
    QueueAndFollow();
}

// State 6 (0x440DA0): a stack table of three - Enter (0x440DD0) bank 0x318,
// poses 0, 0, 0, 0, 0, 0, 0, 2, 1, 3, the tick each frame; Loop (0x440E60):
// with the owner's +0xB set, Sprite_SetAnimation of the same poses and +2 up by
// one (Sprite_Current read after the call), then BattleEnemy_ScriptTick,
// Sprite_QueueOverlay, BossMyriaFx_Follow; Wait (0x440ED0): when
// BattleEnemy_ScriptTickOnce answers al not 0 the owner's +2 up by one and a
// tail jump to BattleTask_FreeCurrent, else Sprite_QueueOverlay and
// BossMyriaFx_Follow.
extern "C" void __cdecl BossMyriaFx_State6(void) {
    static const U kSteps[] = {bof3::addr::BossMyriaFx_State6Enter, bof3::addr::BossMyriaFx_State6Loop,
                               bof3::addr::BossMyriaFx_State6Wait};
    Steps("BossMyriaFx_State6", kSteps, 3);
}

extern "C" void __cdecl BossMyriaFx_State6Enter(void) {
    static const unsigned char kPoses[10] = {0, 0, 0, 0, 0, 0, 0, 2, 1, 3};
    FxEnterPosed("BossMyriaFx_State6Enter", 0x318, kPoses, false);
}

extern "C" void __cdecl BossMyriaFx_State6Loop(void) {
    static const unsigned char kPoses[10] = {0, 0, 0, 0, 0, 0, 0, 2, 1, 3};
    if (Owner()[0xB] != 0) {
        BH_CALL(Sprite_SetAnimation)(Pose("BossMyriaFx_State6Loop", kPoses));
        ++Sprite_Current[2];
    }
    BH_CALL(BattleEnemy_ScriptTick)();
    QueueAndFollow();
}

extern "C" void __cdecl BossMyriaFx_State6Wait(void) {
    if (static_cast<unsigned char>(BH_CALL(BattleEnemy_ScriptTickOnce)()) != 0) {
        ++Owner()[2];
        BH_CALL(BattleTask_FreeCurrent)();
        return;
    }
    QueueAndFollow();
}

// original 0x440EF0 (0x440EF0..0x441087; the tool's cut at 0x44103A is this
// function's own branch): the owner's +0, +0x48, dwords +0x40 / +0x44, +0x27,
// +0x28, +0x2A, words +0x2E / +0x30 / +0x32, +0x5C..+0x5F copied to
// Sprite_Current, in that order; then by its +0x48 the words +0x2E and +0x30
// drift by BossMyriaFx_Drift[+1] (Drift above), the second axis on the
// Sprite_Current of after the first call. Area198_EffectA6Follow (0x42D580)
// is the same shape.
extern "C" void __cdecl BossMyriaFx_Follow(void) {
    {
        const unsigned char* const o = Owner();
        unsigned char* const s = Sprite_Current;
        auto* const cur_task = rv::As<records::BattleTask>(s);
        cur_task->flags = o[0];
        s[0x48] = o[0x48];
        rv::Store<std::int32_t>(cur_task->scale_x, Long(o + 0x40));
        rv::Store<std::int32_t>(cur_task->scale_y, Long(o + 0x44));
        cur_task->clut = o[0x27];
        s[0x28] = o[0x28];
        s[0x2A] = o[0x2A];
        rv::Store<unsigned>(cur_task->screen_x, Word(o + 0x2E));
        rv::Store<unsigned>(cur_task->screen_y, Word(o + 0x30));
        rv::Store<unsigned>(cur_task->draw_key, Word(o + 0x32));
        s[0x5C] = o[0x5C];
        s[0x5D] = o[0x5D];
        s[0x5E] = o[0x5E];
        s[0x5F] = o[0x5F];
    }
    const bool scaled = Sprite_Current[0x48] != 0;
    Drift(scaled, 0, 0x40, 0x2E);
    Drift(scaled, 1, 0x44, 0x30);
}

// original 0x441090 (value, sign): the high word of the 16.16 value - one more
// when sign is not negative and the value's low word is not 0 - in ax; the
// high half of eax is sign's (the original loads it there first).
extern "C" unsigned __cdecl Fixed_HighRoundUp(unsigned value, unsigned sign) {
    const unsigned high = value >> 16;
    if (static_cast<int>(sign) < 0 || (value & 0xFFFF) == 0) return (sign & 0xFFFF0000u) | high;
    return (sign & 0xFFFF0000u) | ((high + 1) & 0xFFFF);
}

void BossMyriaFx_Inject() {
    BOF3_INJECT(BossMyriaFx_Dispatch);
    BOF3_INJECT(BossMyriaFx_State0);
    BOF3_INJECT(BossMyriaFx_State0Enter);
    BOF3_INJECT(BossMyriaFx_State0Loop);
    BOF3_INJECT(BossMyriaFx_State1);
    BOF3_INJECT(BossMyriaFx_State1Enter);
    BOF3_INJECT(BossMyriaFx_State1Loop);
    BOF3_INJECT(BossMyriaFx_State2);
    BOF3_INJECT(BossMyriaFx_State2Enter);
    BOF3_INJECT(BossMyriaFx_State2Loop);
    BOF3_INJECT(BossMyriaFx_State3);
    BOF3_INJECT(BossMyriaFx_State3Enter);
    BOF3_INJECT(BossMyriaFx_State3Wait);
    BOF3_INJECT(BossMyriaFx_State4);
    BOF3_INJECT(BossMyriaFx_State4Enter);
    BOF3_INJECT(BossMyriaFx_State4Wait);
    BOF3_INJECT(BossMyriaFx_State5);
    BOF3_INJECT(BossMyriaFx_State5Enter);
    BOF3_INJECT(BossMyriaFx_State5Wait);
    BOF3_INJECT(BossMyriaFx_State6);
    BOF3_INJECT(BossMyriaFx_State6Enter);
    BOF3_INJECT(BossMyriaFx_State6Loop);
    BOF3_INJECT(BossMyriaFx_State6Wait);
    BOF3_INJECT(BossMyriaFx_Follow);
    BOF3_INJECT(Fixed_HighRoundUp);
}

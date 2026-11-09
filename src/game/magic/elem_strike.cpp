// Moved here by tools/move_funcs.py from magic_s02.cpp (docs/source-layout-round.md).
#include "game/magic/elem_strike.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s02.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "hook/code_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

namespace {

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::Long;

using move_script::SetLong;

using move_script::Word;

// The cells the overlays read beyond the harness's names.
// kActorRecord: now MagicS23_ActorSprite (symbols.toml). unsigned char *: the acting actor's sprite record
constexpr ::bof3::image::Va kAbility{0x904B80};       // u16: the ability being used

// kComboHits: now Rest3c_Hits (symbols.toml). u8: MAGIC003's hit count, for the damage
constexpr ::bof3::image::Va kFxBits{0x904AA9};        // u8: bit 0x20 set by ElemStrike_Start

// kFrameSet: now BattleE2_AnimSet (symbols.toml). the sprite frame-offset table pointer (sprite_pose.h)
constexpr ::bof3::image::Va kFrameSetBattle{0x8B3580};

constexpr ::bof3::image::Va kFrameSetEffect{0x8E3580};

constexpr ::bof3::image::Va kStrikeTints{0x65A584};   // MAGIC004: 3 bytes a kind

constexpr ::bof3::image::Va kFxDelays{0x65A5A8};      // bytes: the effect's sound delay, by kind

constexpr ::bof3::image::Va kStrikeSounds{0x65A5B4};  // words: the hit sound, by kind (0 none)

// ElemStrike_Kind's case by the ability word less 4: the switch the compiler
// made the byte table at kKindIndex of - the function's logic, ours since the
// function is (docs/operand-store.md). Eleven abilities have a case of their
// own (0..10); every other index, and any past the table, is case 11.
unsigned KindCase(std::uint32_t index) {
    switch (index) {
    case 0x00: return 0;
    case 0x01: return 1;
    case 0x03: return 2;
    case 0x19: return 3;
    case 0x2D: return 4;
    case 0x81: return 5;
    case 0x82: return 6;
    case 0x83: return 7;
    case 0x84: return 8;
    case 0x97: return 9;
    case 0x98: return 10;
    default: return 11;
    }
}

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

unsigned ActorByte() { return static_cast<unsigned>(Long(Mem(::bof3::Img(at::kActor)))) & 0xFF; }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

std::uint32_t Key(const void* p) { return Low32(p); }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

// The originals index the records by the battle index, unchecked: a party
// member below 3 (the target) or at most 2 (the actor), else the enemy by
// index - 3.
unsigned char* PartyRecord(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }

unsigned char* EnemyRecord(unsigned battle_index) {
    return Mem(::bof3::Img(Battle_Enemies_addr) + static_cast<std::uint32_t>(static_cast<int>(battle_index) - 3) * at::kEnemyStride);
}

unsigned char* TargetRecord(unsigned t) { return t < 3 ? PartyRecord(t) : EnemyRecord(t); }

unsigned char* ActorRecord(unsigned a) { return a <= 2 ? PartyRecord(a) : EnemyRecord(a); }

// `mov ecx, 0x20; rep movsd`: 0x80 bytes, dword by dword, forward.
void CopyRecord(unsigned char* to, const unsigned char* from) {
    for (unsigned k = 0; k < 0x80; k += 4) {
        std::uint32_t v;
        std::memcpy(&v, from + k, 4);
        std::memcpy(to + k, &v, 4);
    }
}

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// A draw-mode packet (tpage `tpage`, dithered) committed to `layer`.
void DrawModeCommit(unsigned tpage, unsigned layer) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
    MH_CALL(Gfx_CommitPrim)(layer, 0xC);
}

// A counter byte down by one; true when it reached 0.
bool CountDown(unsigned k) {
    Dec(Sc()[k]);
    return Sc()[k] == 0;
}

unsigned char Tick() { return MH_CALL(Sprite_ScriptTickOnce)(); }

}  // namespace

#define S02_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x49BAF0: the kind-2 task, a five-entry stack table by +1.
S02_EXPORT void __cdecl ElemStrike_Task(void) {
    static constexpr std::uint32_t kPhases[5] = {bof3::addr::ElemStrike_Start, bof3::addr::ElemStrike_Tint,
                                                 bof3::addr::ElemStrike_Hit, bof3::addr::ElemStrike_Fade,
                                                 bof3::addr::ElemStrike_End};
    const unsigned phase = Sc()[1];
    if (phase >= 5) PastTable("ElemStrike_Task", phase, 5);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x49BB30: the owner's direction and position; +0xB and +9 0, +1 on;
// the caster's animation 0xC; the kind (+3 / +4, ElemStrike_Kind); a child of
// kind 1, parameter 0x46, made a copy of the acting actor's record (+1 0: it
// plays), and a second (+1 1: the effect, with this task's +3 / +4), both
// owned by this task and counted in +0xB (slots unchecked); the owner's +0
// bit 0x40, 0x904AA9 bit 0x20; CLUT row 26 back from its source.
S02_EXPORT void __cdecl ElemStrike_Start(void) {
    Sc()[8] = Owner()[8];
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    Inc(Sc()[1]);
    MH_CALL(BattleActor_SetAnimation)(0xC, 2);
    Call0(bof3::addr::ElemStrike_Kind);
    {
        const unsigned slot = MH_CALL(BattleTask_Create)(1, 0x46) & 0xFFu;
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        CopyRecord(child, ActorRecord(ActorByte()));
        unsigned char* const s = Sc();
        tchild->owner = static_cast<std::int32_t>(Key(s));
        child[1] = 0;
        child[2] = 0;
        child[6] = 1;
        child[5] = 0x46;
        s[0xB] = static_cast<unsigned char>(s[0xB] + 1);
    }
    {
        const unsigned slot = MH_CALL(BattleTask_Create)(1, 0x46) & 0xFFu;
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        unsigned char* const s = Sc();
        tchild->owner = static_cast<std::int32_t>(Key(s));
        child[1] = 1;
        child[3] = s[3];
        child[4] = s[4];
        Inc(s[0xB]);
    }
    Owner()[0] |= 0x40;
    Mem(::bof3::Img(kFxBits.v))[0] |= 0x20;
    for (unsigned k = 0x1A00; k < 0x1B00; ++k) Gfx_ClutStrip[k] = Gfx_ClutStripSource[k];
    Gfx_ClutStripDirty = 1;
}

// original 0x49BCD0: once +0xB is at most 1 (the copy's child gone): the
// kind's three tint bytes (0x65A584 + 3 x +4) to +0x5D..+0x5F; the target's
// record (taken first) untinted and tinted by them, the slot to +0xA; the
// caster's animation 4; the owner's bit 0x40 cleared; target flags 0x20;
// sound 0x100, and 0x101 for an ability 0x85..0x88; +1 on.
S02_EXPORT void __cdecl ElemStrike_Tint(void) {
    if (Sc()[0xB] > 1) return;
    unsigned char* const record = TargetRecord(static_cast<unsigned>(Long(Mem(::bof3::Img(at::kTarget)))) & 0xFF);
    for (unsigned c = 0; c < 3; ++c) {
        unsigned char* const s = Sc();
        s[0x5D + c] = Mem(::bof3::Img(kStrikeTints.v) + 3u * s[4] + c)[0];
    }
    MH_CALL(Sprite_ReleaseTint)(record);
    const unsigned char* const s = Sc();
    const unsigned char slot = MH_CALL(Sprite_SetTint)(record, s[0x5D], s[0x5E], s[0x5F], 0);
    Sc()[0xA] = slot;
    MH_CALL(BattleActor_SetAnimation)(4, 0);
    Owner()[0] &= 0xBF;
    MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x20);
    MH_CALL(Sound_PlayById)(0x100);
    const std::uint16_t ability = Word(Mem(::bof3::Img(kAbility.v)));
    if (ability > 0x84 && ability <= 0x88) MH_CALL(Sound_PlayById)(0x101);
    Inc(Sc()[1]);
}

// original 0x49BDE0: once +0xB is 0: the kind's hit sound (0x65A5B4 + 2 x +4)
// unless 0; target flag 0x40; +9 0, +1 on.
S02_EXPORT void __cdecl ElemStrike_Hit(void) {
    const unsigned char* const s = Sc();
    if (s[0xB] != 0) return;
    const std::uint16_t sound = Word(Mem(::bof3::Img(kStrikeSounds.v) + 2u * s[4]));
    if (sound != 0) MH_CALL(Sound_PlayById)(sound);
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    Sc()[9] = 0;
    Inc(Sc()[1]);
}

// original 0x49BE30: while +9 is below 0xC, MAGIC008's 0x4A29C0 and +9 up by
// 4; each of +0x5D..+0x5F not 0 down by 2, written to tint record +0xA; all
// three 0: the target's record untinted, the target flashed, +1 on.
S02_EXPORT void __cdecl ElemStrike_Fade(void) {
    if (Sc()[9] < 0xC) {
        Call0(bof3::addr::Magic008_DrawFlash);
        Sc()[9] = static_cast<unsigned char>(Sc()[9] + 4);
    }
    unsigned char* const s = Sc();
    for (unsigned c = 0x5D; c < 0x60; ++c)
        if (s[c] != 0) s[c] = static_cast<unsigned char>(s[c] - 2);
    for (unsigned c = 0; c < 3; ++c) MoveScript_TintRecords[s[0xA] * 12u + 2 + c] = s[0x5D + c];
    if (s[0x5D] != 0 || s[0x5E] != 0 || s[0x5F] != 0) return;
    MH_CALL(Sprite_ReleaseTint)(TargetRecord(static_cast<unsigned>(Long(Mem(::bof3::Img(at::kTarget)))) & 0xFF));
    MH_CALL(BattleActor_Flash)(TargetByte());
    Inc(Sc()[1]);
}

// original 0x49BF30: the target's record taken first; the effect ends (the done
// bit, the task freed) unless the target is not out and its state +1 is 6.
S02_EXPORT void __cdecl ElemStrike_End(void) {
    const unsigned char target = TargetByte();
    const unsigned char* const record = TargetRecord(static_cast<unsigned>(Long(Mem(::bof3::Img(at::kTarget)))) & 0xFF);
    if (MH_CALL(Battle_ActorIsOut)(target) == 0 && record[1] == 6) return;
    Mem(::bof3::Img(at::kFlags))[0] |= 4;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x49BF90: the ability word 0x904B80 less 4, through the byte table
// 0x49C0CC (0x99 entries, past it the last case), picks one of twelve cases:
// +3 / +4 as the jump table's targets write them.
S02_EXPORT void __cdecl ElemStrike_Kind(void) {
    static constexpr unsigned char kCases[12][2] = {{0, 0}, {1, 1}, {0, 2},   {1, 3},   {0, 4}, {1, 8},
                                                    {1, 9}, {1, 0xA}, {1, 0xB}, {1, 5}, {0, 6}, {0, 7}};
    const std::uint32_t index = (static_cast<std::uint32_t>(Long(Mem(::bof3::Img(kAbility.v)))) & 0xFFFF) - 4u;
    const unsigned c = KindCase(index);   // past 0x98 the last case, as the original's bound
    if (c >= 12) PastTable("ElemStrike_Kind", c, 12);
    Sc()[3] = kCases[c][0];
    Sc()[4] = kCases[c][1];
}

// original 0x49C170: the kind-1 task of parameter 0x46, a jmp through
// ElemStrikeChild_Kinds (two entries) by +1, unchecked.
S02_EXPORT void __cdecl ElemStrikeChild_Task(void) {
    static constexpr std::uint32_t kKinds[2] = {bof3::addr::ElemStrikeCopy_Run, bof3::addr::ElemStrikeFx_Run};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("ElemStrikeChild_Task", phase, 2);
    magic_harness::Phase(kKinds[phase])();
}

// original 0x49C190: the caster's copy, a four-entry stack table by +2
// (BattleFx_SetSize, ElemStrikeCopy_Play, MAGIC161's 0x4EE560,
// BattleFx_FreeTask); the sprite's screen point updated while +0 is set.
S02_EXPORT void __cdecl ElemStrikeCopy_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::BattleFx_SetSize, bof3::addr::ElemStrikeCopy_Play,
                                                bof3::addr::BattleFx_ScriptToEnd, bof3::addr::BattleFx_FreeTask};
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("ElemStrikeCopy_Run", phase, 4);
    magic_harness::Phase(kSteps[phase])();
    if (Sc()[0] != 0) MH_CALL(Sprite_UpdateScreen)();
}

// original 0x49C1E0: the script ticked; +9 down, at 0 the actor's sound (2 for
// an ability of 0x85 or more, else 0; 4) and +2 on.
S02_EXPORT void __cdecl ElemStrikeCopy_Play(void) {
    Tick();
    if (!CountDown(9)) return;
    MH_CALL(BattleActor_PlaySound)(Word(Mem(::bof3::Img(kAbility.v))) >= 0x85 ? 2 : 0, 4);
    Inc(Sc()[2]);
}

// original 0x49C230: a draw-mode packet on layer 3; the effects' frame-offset
// table (0x8E3580) while a call through ElemStrikeFx_Steps (three entries) by
// +2 runs; the battle's back.
S02_EXPORT void __cdecl ElemStrikeFx_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::MagicFx_WaitOwnerChildren, bof3::addr::ElemStrikeFx_Start,
                                                bof3::addr::ElemStrikeFx_Play};
    DrawModeCommit(0x15, 3);
    const unsigned phase = Sc()[2];
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), static_cast<std::int32_t>(::bof3::Img(kFrameSetEffect.v)));
    if (phase >= 3) PastTable("ElemStrikeFx_Run", phase, 3);
    magic_harness::Phase(kSteps[phase])();
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), static_cast<std::int32_t>(::bof3::Img(kFrameSetBattle.v)));
}

// original 0x49C280: at the source sprite, 0x800000 higher; the sprite bytes
// (+0x27 0x1A, +0x28 1, +0x24 0 for a kind with +3, else 0xA0, 0, 4; +0x25
// 0x1D, +0x26 0, +0x5C..+0x5F 0, +0x2A 0, +0x29 4, +0x2C 0, +0x2B 1);
// animation 0; +9 the kind's delay (0x65A5A8 + +4); +2 on.
S02_EXPORT void __cdecl ElemStrikeFx_Start(void) {
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Pointer(::bof3::Img(BattleOdds_ActorObject_addr)))->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Pointer(::bof3::Img(BattleOdds_ActorObject_addr)))->z);
    rv::Store(rv::Current<records::BattleTask>()->height, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Pointer(::bof3::Img(BattleOdds_ActorObject_addr)))->height) + 0x800000u));
    unsigned char* const s = Sc();
    s[0x25] = 0x1D;
    s[0x26] = 0;
    if (s[3] != 0) {
        s[0x27] = 0x1A;
        s[0x28] = 1;
        s[0x24] = 0;
    } else {
        s[0x27] = 0xA0;
        s[0x28] = 0;
        s[0x24] = 4;
    }
    s[0x5D] = 0;
    s[0x5E] = 0;
    s[0x5F] = 0;
    s[0x5C] = 0;
    s[0x2A] = 0;
    s[0x29] = 4;
    rv::Store<unsigned>(rv::As<records::BattleTask>(s)->column, 0);
    s[0x2B] = 1;
    MH_CALL(Sprite_SetAnimation)(0);
    Sc()[9] = Mem(::bof3::Img(kFxDelays.v))[Sc()[4]];
    Inc(Sc()[2]);
}

// original 0x49C380: +9 counts down to 0, at 0 sound 0x101; the script ticked:
// at its end the owner's +0xB down and the task freed, else the sprite's
// screen point updated (both tail jmps).
S02_EXPORT void __cdecl ElemStrikeFx_Play(void) {
    unsigned char* const s = Sc();
    const unsigned char delay = s[9];
    if (delay != 0) {
        s[9] = static_cast<unsigned char>(delay - 1);
        if (Sc()[9] == 0) MH_CALL(Sound_PlayById)(0x101);
    }
    if (Tick() != 0) {
        Dec(Owner()[0xB]);
        MH_CALL(BattleTask_FreeCurrent)();
        return;
    }
    MH_CALL(Sprite_UpdateScreen)();
}

void ElemStrike_Inject() {
    BOF3_INJECT(ElemStrike_Task);
    BOF3_INJECT(ElemStrike_Start);
    BOF3_INJECT(ElemStrike_Tint);
    BOF3_INJECT(ElemStrike_Hit);
    BOF3_INJECT(ElemStrike_Fade);
    BOF3_INJECT(ElemStrike_End);
    BOF3_INJECT(ElemStrike_Kind);
    BOF3_INJECT(ElemStrikeChild_Task);
    BOF3_INJECT(ElemStrikeCopy_Run);
    BOF3_INJECT(ElemStrikeCopy_Play);
    BOF3_INJECT(ElemStrikeFx_Run);
    BOF3_INJECT(ElemStrikeFx_Start);
    BOF3_INJECT(ElemStrikeFx_Play);
}

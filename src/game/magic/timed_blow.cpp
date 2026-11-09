// Moved here by tools/move_funcs.py from magic_s34.cpp (docs/source-layout-round.md).
#include "game/magic/timed_blow.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s34.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

namespace {

namespace addr = bof3::addr;

using magic_harness::Mem;

using magic_harness::Pointer;

constexpr auto& LongMagicS34 = move_script::Long;

using move_script::SetLong;

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

std::uint32_t Key(const void* p) { return Low32(p); }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

// The originals index the records by the battle index, unchecked: the party
// member's by index, the enemy's by index - 3.
unsigned char* PartyRecord(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }

unsigned char* EnemyRecord(unsigned battle_index) {
    return Mem(::bof3::Img(Battle_Enemies_addr) + static_cast<std::uint32_t>(static_cast<int>(battle_index) - 3) * at::kEnemyStride);
}

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code), in the fuzz that
// address's recorder.
using Fn0 = void (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

void Dispatch(const std::uint32_t* table, unsigned entries, unsigned phase, const char* who) {
    if (phase >= entries) PastTable(who, phase, entries);
    magic_harness::Phase(table[phase])();
}

// A draw-mode packet (tpage 0x35, dithered) at Gfx_PacketNext.
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

// The raw floats of TimedBlow_DrawFlash's corners: 120.0 and 359.0 (its 319.0
// is Widescreen_FillRight(319.0f), DIV-0041).
constexpr std::int32_t kMiddle = 0x42F00000;

constexpr std::int32_t kBottom = 0x43B38000;

void Shade(unsigned char* p, unsigned k, unsigned char r, unsigned char g) {
    p[k] = r;
    p[k + 1] = g;
    p[k + 2] = g;
}

}  // namespace

#define S34_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4EE2E0: the kind-2 task. A three-entry stack table by +1
// (TimedBlow_Start, TimedBlow_Wait, MagicFx_DoneAndFree).
S34_EXPORT void __cdecl TimedBlow_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {addr::TimedBlow_Start, addr::TimedBlow_Wait, addr::MagicFx_DoneAndFree};
    Dispatch(kPhases, 3, Sc()[1], "TimedBlow_Task");
}

// original 0x4EE310: the task at the owner (its facing and point), +0xB 0,
// +1 on; the actor animation 0xC (BattleActor_SetAnimation(0xC, 2)); a child
// (kind 1, 0x54) that is a copy of the acting actor's record's first 0x80
// bytes (a party member at 0..2, else the enemy by index - 3, unchecked),
// owned by this task, +1 / +2 0, +6 1, +5 0x54, counted in +0xB; the owner's
// +0 bit 0x40 set.
S34_EXPORT void __cdecl TimedBlow_Start(void) {
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        s[8] = Owner()[8];
        ts->x = rv::As<records::SpriteHead>(Owner())->x;
        ts->z = rv::As<records::SpriteHead>(Owner())->z;
        rv::Store(ts->height, rv::As<records::SpriteHead>(Owner())->height);
        s[0xB] = 0;
        Inc(s[1]);
    }
    MH_CALL(BattleActor_SetAnimation)(0xC, 2);
    const unsigned slot = NewTask(0x54);
    const unsigned actor = Mem(::bof3::Img(at::kActor))[0];
    const unsigned char* const rec = actor <= 2 ? PartyRecord(actor) : EnemyRecord(actor);
    unsigned char* const child = TaskSlot(slot);
    records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
    // `rep movsd`: dword by dword, forward.
    for (unsigned k = 0; k < 0x80; k += 4) SetLong(child + k, LongMagicS34(rec + k));
    unsigned char* const self = Sc();
    tchild->owner = static_cast<std::int32_t>(Key(self));
    child[1] = 0;
    child[2] = 0;
    child[6] = 1;
    child[5] = 0x54;
    Inc(self[0xB]);
    Owner()[0] = static_cast<unsigned char>(Owner()[0] | 0x40);
}

// original 0x4EE430: once the child count +0xB is 0, the owner's bit 0x40
// cleared, the actor animation 0x18 (BattleActor_SetAnimation(0x18, 0)), +1
// on.
S34_EXPORT void __cdecl TimedBlow_Wait(void) {
    if (Sc()[0xB] != 0) return;
    Owner()[0] = static_cast<unsigned char>(Owner()[0] & 0xBF);
    MH_CALL(BattleActor_SetAnimation)(0x18, 0);
    Inc(Sc()[1]);
}

// original 0x4EE460: the copy's kind-1 task, a jmp through
// TimedBlowCopy_TaskTable (one entry) by +1, unchecked.
S34_EXPORT void __cdecl TimedBlowCopy_Task(void) {
    static constexpr std::uint32_t kKinds[1] = {addr::TimedBlowCopy_Run};
    Dispatch(kKinds, 1, Sc()[1], "TimedBlowCopy_Task");
}

// original 0x4EE480: a five-entry stack table by +2 (BattleFx_SetSize,
// TimedBlowCopy_Strike, TimedBlowCopy_Flash, BattleFx_ScriptToEnd,
// BattleFx_FreeTask); the screen update while +0 and +2 are set.
S34_EXPORT void __cdecl TimedBlowCopy_Run(void) {
    static constexpr std::uint32_t kSteps[5] = {addr::BattleFx_SetSize, addr::TimedBlowCopy_Strike,
                                                addr::TimedBlowCopy_Flash, addr::BattleFx_ScriptToEnd,
                                                addr::BattleFx_FreeTask};
    Dispatch(kSteps, 5, Sc()[2], "TimedBlowCopy_Run");
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(Sprite_UpdateScreen)();
}

// original 0x4EE4E0: the script ticked once (its answer unread); +9 down, at
// 0 the actor sound (BattleActor_PlaySound(2, 4)), the target flagged 0x40,
// +2 on.
S34_EXPORT void __cdecl TimedBlowCopy_Strike(void) {
    MH_CALL(Sprite_ScriptTickOnce)();
    unsigned char* const s = Sc();
    Dec(s[9]);
    if (s[9] != 0) return;
    MH_CALL(BattleActor_PlaySound)(2, 4);
    MH_CALL(Battle_SetTargetFlag40)(Mem(::bof3::Img(at::kTarget))[0]);
    Inc(Sc()[2]);
}

// original 0x4EE520: at +9 0x10 sound 0x100 and +2 on, else +9 up by 4; the
// flash drawn, the script ticked once (a tail jmp).
S34_EXPORT void __cdecl TimedBlowCopy_Flash(void) {
    unsigned char* const s = Sc();
    const unsigned char n = s[9];
    if (n == 0x10) {
        MH_CALL(Sound_PlayById)(0x100);
        Inc(Sc()[2]);
    } else {
        s[9] = static_cast<unsigned char>(n + 4);
    }
    Call0(addr::TimedBlow_DrawFlash);
    MH_CALL(Sprite_ScriptTickOnce)();
}

// original 0x4EE560, shared by eleven files: the script ticked once; when it
// reports its end, the owner's count +0xB down and +2 on.
S34_EXPORT void __cdecl BattleFx_ScriptToEnd(void) {
    if ((MH_CALL(Sprite_ScriptTickOnce)() & 0xFF) == 0) return;
    Dec(Owner()[0xB]);
    Inc(Sc()[2]);
}

// original 0x4EE580: a draw-mode packet (tpage 0x35) on layer 1; two
// semi-transparent gouraud quads over the screen, (0, 0)..(319, 120) and
// (0, 120)..(319, 359), red (0xC0, 0x60, 0x60) at the top and bottom edges
// and +9 x (0xC, 6, 6) where they meet; a closing draw-mode packet (tpage
// 0x15). The corners are float constants. Under DIV-0041 x runs from
// Widescreen_FillX() to 319 + the columns (the PSP's -32..351); 0..319 while
// the fills are unarmed, as the fuzz compares.
S34_EXPORT void __cdecl TimedBlow_DrawFlash(void) {
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(1, 0xC);
    const unsigned char n = Sc()[9];
    unsigned char* p = Gfx_PacketNext;
    const auto edge = static_cast<unsigned char>(n * 0xCu);
    const auto mid = static_cast<unsigned char>(n * 6u);
    const auto left = std::bit_cast<std::int32_t>(Widescreen_FillX());
    const auto right = std::bit_cast<std::int32_t>(Widescreen_FillRight(319.0f));
    MH_CALL(Gpu_SetPolyG4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    SetLong(p + 0x18, right);
    SetLong(p + 0x38, right);
    SetLong(p + 8, left);
    SetLong(p + 0xC, 0);
    SetLong(p + 0x1C, 0);
    SetLong(p + 0x28, left);
    SetLong(p + 0x2C, kMiddle);
    SetLong(p + 0x3C, kMiddle);
    Shade(p, 4, 0xC0, 0x60);
    Shade(p, 0x14, 0xC0, 0x60);
    Shade(p, 0x24, edge, mid);
    Shade(p, 0x34, edge, mid);
    MH_CALL(Gfx_CommitPrim)(1, 0x44);
    p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyG4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    SetLong(p + 0x2C, kBottom);
    SetLong(p + 0x3C, kBottom);
    SetLong(p + 8, left);
    SetLong(p + 0xC, kMiddle);
    SetLong(p + 0x18, right);
    SetLong(p + 0x1C, kMiddle);
    SetLong(p + 0x28, left);
    SetLong(p + 0x38, right);
    Shade(p, 4, edge, mid);
    Shade(p, 0x14, edge, mid);
    Shade(p, 0x24, 0xC0, 0x60);
    Shade(p, 0x34, 0xC0, 0x60);
    MH_CALL(Gfx_CommitPrim)(1, 0x44);
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(1, 0xC);
}

void TimedBlow_Inject() {
    BOF3_INJECT(TimedBlow_Task);
    BOF3_INJECT(TimedBlow_Start);
    BOF3_INJECT(TimedBlow_Wait);
    BOF3_INJECT(TimedBlowCopy_Task);
    BOF3_INJECT(TimedBlowCopy_Run);
    BOF3_INJECT(TimedBlowCopy_Strike);
    BOF3_INJECT(TimedBlowCopy_Flash);
    BOF3_INJECT(BattleFx_ScriptToEnd);
    BOF3_INJECT(TimedBlow_DrawFlash);
}

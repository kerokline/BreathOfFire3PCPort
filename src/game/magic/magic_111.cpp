// Moved here by tools/move_funcs.py from magic_c3.cpp (docs/source-layout-round.md).
#include "game/magic/magic_111.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_c3.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

namespace {

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

std::int32_t AddU(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}

// The scratch words MAGIC111's wash keeps its angles in: SharedScratch
// 0x903850 and 0x903852, read back after every Math_Sin as the original does.
constexpr ::bof3::image::Va kAngleA{SharedScratch_addr};

constexpr ::bof3::image::Va kAngleB{0x903852};

using Fn0 = void (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

}  // namespace

#define C3_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// One colour byte of the wash: ((sin(angle) x 6 >> 12) + 8) x +9, both bytes
// (the original's `add al, 8; imul byte [+9]`), +9 read after the call.
unsigned char Pulse(int angle) {
    const int v = MH_CALL(Math_Sin)(angle);
    const auto t = static_cast<unsigned char>((static_cast<int>(static_cast<std::uint32_t>(v) * 6u) >> 12) + 8);
    return static_cast<unsigned char>(t * Sc()[9]);
}

short AngleWord(std::uint32_t cell) { return static_cast<short>(Word(Mem(cell))); }

unsigned SetAngle(std::uint32_t cell, unsigned byte, unsigned add) {
    const unsigned a = ((byte + add) & 0x3F) << 6;
    SetWord(Mem(cell), a);
    return a;
}

// The quad's corners (0, 0)..(319, 239); under DIV-0041 x runs from
// Widescreen_FillX() to 319 + the columns (the PSP's -32..351), 0..319 while
// the fills are unarmed, so the fuzz compares the original's.
void FullScreen(unsigned char* p, unsigned x1, unsigned y1, unsigned x2, unsigned y2, unsigned x3, unsigned y3) {
    constexpr std::uint32_t k239 = 0x436F0000;   // 239.0f
    const auto left = std::bit_cast<std::int32_t>(Widescreen_FillX());
    const auto right = std::bit_cast<std::int32_t>(Widescreen_FillRight(319.0f));
    SetLong(p + 8, left);
    SetLong(p + 0xC, 0);
    SetLong(p + x1, right);
    SetLong(p + y1, 0);
    SetLong(p + x2, left);
    SetLong(p + y2, static_cast<std::int32_t>(k239));
    SetLong(p + x3, right);
    SetLong(p + y3, static_cast<std::int32_t>(k239));
}

}  // namespace

// original 0x4D6110: the kind-2 task. A three-entry stack table by +1:
// Magic111_Start, Magic111_Wait, the engine's 0x43FE80.
C3_EXPORT void __cdecl Magic111_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::Magic111_Start, bof3::addr::Magic111_Wait,
                                                 bof3::addr::MagicFx_DoneAndFree};
    const unsigned phase = Sc()[1];
    if (phase >= 3) PastTable("Magic111_Task", phase, 3);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4D6140: the task at the owner (direction byte +8, position
// +0x34..+0x3C), +0xB and +9 0, +1 on; three kind-1 children of 0x57, each
// counted in +0xB with +0x80 this task: the first a copy of the acting actor's
// record (0x904B34: party below 3, else enemy - 3; its first 0x80 bytes) with
// +1 and +2 0, kind 1 and parameter 0x57 put back; the second +1 = 1, the third
// +1 = 2. Sounds 0x100 and 0x101; the owner's +0 bit 0x40 set.
C3_EXPORT void __cdecl Magic111_Start(void) {
    Sc()[8] = Owner()[8];
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    Inc(Sc()[1]);
    unsigned slot = MH_CALL(BattleTask_Create)(1, 0x57) & 0xFFu;
    const unsigned actor = Mem(::bof3::Img(at::kActor))[0];
    const unsigned char* const record =
        actor <= 2 ? Mem(::bof3::Img(ObjTrio_addr) + actor * at::kPartyStride) : Mem(::bof3::Img(Battle_Enemies_addr) + (actor - 3) * at::kEnemyStride);
    unsigned char* child = TaskSlot(slot);
    std::memmove(child, record, 0x80);
    unsigned char* self = Sc();
    rv::Store<std::int32_t>(rv::As<records::BattleTask>(child)->owner, static_cast<std::int32_t>(Low32(self)));
    child[1] = 0;
    child[2] = 0;
    child[6] = 1;
    child[5] = 0x57;
    Inc(self[0xB]);
    for (unsigned kind = 1; kind <= 2; ++kind) {
        slot = MH_CALL(BattleTask_Create)(1, 0x57) & 0xFFu;
        child = TaskSlot(slot);
        self = Sc();
        rv::Store<std::int32_t>(rv::As<records::BattleTask>(child)->owner, static_cast<std::int32_t>(Low32(self)));
        child[1] = static_cast<unsigned char>(kind);
        Inc(self[0xB]);
    }
    MH_CALL(Sound_PlayById)(0x100);
    MH_CALL(Sound_PlayById)(0x101);
    Owner()[0] |= 0x40;
}

// original 0x4D62C0: once +0xB (the children alive) is 0: animation 4 on the
// caster, the owner's +0 bit 0x40 cleared, the target flag 0x40, +1 on.
C3_EXPORT void __cdecl Magic111_Wait(void) {
    if (Sc()[0xB] != 0) return;
    MH_CALL(BattleActor_SetAnimation)(4, 0);
    Owner()[0] &= 0xBF;
    MH_CALL(Battle_SetTargetFlag40)(Mem(::bof3::Img(at::kTarget))[0]);
    Inc(Sc()[1]);
}

// original 0x4D6300: the children's kind-1 task, a jmp through
// Magic111Child_Kinds (three entries) by +1, unchecked.
C3_EXPORT void __cdecl Magic111Child_Task(void) {
    static constexpr std::uint32_t kKinds[3] = {bof3::addr::Magic111Double_Run, bof3::addr::Magic111Wash_Run,
                                                bof3::addr::Magic111Flash_Run};
    const unsigned kind = Sc()[1];
    if (kind >= 3) PastTable("Magic111Child_Task", kind, 3);
    magic_harness::Phase(kKinds[kind])();
}

// original 0x4D6320: the caster's copy. A three-entry stack table by +2 -
// Magic111Double_Begin, Magic111Double_Wait, BattleFx_FreeTask - then
// Sprite_UpdateScreen while +0 is set.
C3_EXPORT void __cdecl Magic111Double_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::Magic111Double_Begin, bof3::addr::Magic111Double_Wait,
                                                bof3::addr::BattleFx_FreeTask};
    const unsigned step = Sc()[2];
    if (step >= 3) PastTable("Magic111Double_Run", step, 3);
    magic_harness::Phase(kSteps[step])();
    if (Sc()[0] != 0) MH_CALL(Sprite_UpdateScreen)();
}

// original 0x4D6360: +0x29 = 2, +2 on.
C3_EXPORT void __cdecl Magic111Double_Begin(void) {
    Sc()[0x29] = 2;
    Inc(Sc()[2]);
}

// original 0x4D6380: once the owner's +0xB is 1 or less, it goes down by one
// (0 wraps to 0xFF, as in the original) and +2 on.
C3_EXPORT void __cdecl Magic111Double_Wait(void) {
    unsigned char* const owner = Owner();
    const unsigned char count = owner[0xB];
    if (count > 1) return;
    owner[0xB] = static_cast<unsigned char>(count - 1);
    Inc(Sc()[2]);
}

// original 0x4D63A0: the wash. A call through Magic111Wash_Steps (four
// entries) by +2 - Magic111Wash_Start, BarrierRing_Grow, MagicFx_WaitA,
// MAGIC060's 0x4B1740 -, then while +0 is set: the two angle dwords +0xC and
// +0x10 up by one and the draw (a tail jmp).
C3_EXPORT void __cdecl Magic111Wash_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::Magic111Wash_Start, bof3::addr::BarrierRing_Grow,
                                                bof3::addr::MagicFx_WaitA, bof3::addr::MagicFx_CountDownRelease};
    const unsigned step = Sc()[2];
    if (step >= 4) PastTable("Magic111Wash_Run", step, 4);
    magic_harness::Phase(kSteps[step])();
    if (Sc()[0] == 0) return;
    rv::Store(rv::Current<records::BattleTask>()->step_x, AddU(rv::Current<records::BattleTask>()->step_x, 1));
    rv::Store(rv::Current<records::BattleTask>()->step_z, AddU(rv::Current<records::BattleTask>()->step_z, 1));
    Call0(bof3::addr::Magic111Wash_Draw);
}

// original 0x4D63E0: +0xC = Rand & 0x3F, +0x10 = it + 0x1F (dwords), +9 0,
// +0xA 0x3C, +2 on.
C3_EXPORT void __cdecl Magic111Wash_Start(void) {
    const std::uint32_t r = static_cast<std::uint32_t>(MH_CALL(Rand)());
    rv::Store(rv::Current<records::BattleTask>()->step_x, static_cast<std::int32_t>(r & 0x3F));
    rv::Store(rv::Current<records::BattleTask>()->step_z, AddU(rv::Current<records::BattleTask>()->step_x, 0x1F));
    Sc()[9] = 0;
    Sc()[0xA] = 0x3C;
    Inc(Sc()[2]);
}

// original 0x4D6420: tpage 0x55 (subtractive) as a draw mode; a
// semi-transparent gouraud quad over the whole screen (0,0)..(319,239), each
// corner's red, green and blue a Pulse of an angle from +0xC / +0x10 (offset
// 0, 0xF, 0x1F, -0x11 by corner, & 0x3F << 6, kept in 0x903850 / 0x903852 and
// read back from there after the first call of each corner, in the original's
// order); then tpage 0x15 as a draw mode.
C3_EXPORT void __cdecl Magic111Wash_Draw(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x55, 0);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyG4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    FullScreen(p, 0x18, 0x1C, 0x28, 0x2C, 0x38, 0x3C);

    unsigned a = SetAngle(::bof3::Img(kAngleA.v), Sc()[0xC], 0);
    SetAngle(::bof3::Img(kAngleB.v), Sc()[0x10], 0);
    unsigned char c0 = Pulse(static_cast<short>(a));
    unsigned char c1 = Pulse(AngleWord(::bof3::Img(kAngleB.v)));
    unsigned char c2 = Pulse(AngleWord(::bof3::Img(kAngleB.v)));
    p[4] = c0;
    p[5] = c1;
    p[6] = c2;

    SetAngle(::bof3::Img(kAngleA.v), Sc()[0xC], 0xF);
    unsigned b = SetAngle(::bof3::Img(kAngleB.v), Sc()[0x10], 0xF);
    c0 = Pulse(static_cast<short>(b));
    c1 = Pulse(AngleWord(::bof3::Img(kAngleA.v)));
    c2 = Pulse(AngleWord(::bof3::Img(kAngleB.v)));
    p[0x14] = c0;
    p[0x15] = c1;
    p[0x16] = c2;

    SetAngle(::bof3::Img(kAngleA.v), Sc()[0xC], 0x1F);
    b = SetAngle(::bof3::Img(kAngleB.v), Sc()[0x10], 0x1F);
    c0 = Pulse(static_cast<short>(b));
    c1 = Pulse(AngleWord(::bof3::Img(kAngleB.v)));
    c2 = Pulse(AngleWord(::bof3::Img(kAngleA.v)));
    p[0x24] = c0;
    p[0x25] = c1;
    p[0x26] = c2;

    a = SetAngle(::bof3::Img(kAngleA.v), Sc()[0xC], 0x100 - 0x11);
    SetAngle(::bof3::Img(kAngleB.v), Sc()[0x10], 0x100 - 0x11);
    c0 = Pulse(static_cast<short>(a));
    c1 = Pulse(AngleWord(::bof3::Img(kAngleA.v)));
    c2 = Pulse(AngleWord(::bof3::Img(kAngleB.v)));
    p[0x34] = c0;
    p[0x35] = c1;
    p[0x36] = c2;
    MH_CALL(Gfx_CommitPrim)(3, 0x44);
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
}

// original 0x4D6700: the flash. A call through Magic111Flash_Steps (three
// entries) by +2 - Magic111Flash_Wait, MAGIC131's 0x4E5950, MAGIC060's
// 0x4B18B0 -, then while +0 and +2 are set the draw (a tail jmp).
C3_EXPORT void __cdecl Magic111Flash_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::Magic111Flash_Wait, bof3::addr::MagicFx_CountUp9By2,
                                                bof3::addr::MagicFx_CountDown2Release};
    const unsigned step = Sc()[2];
    if (step >= 3) PastTable("Magic111Flash_Run", step, 3);
    magic_harness::Phase(kSteps[step])();
    if (Sc()[0] == 0 || Sc()[2] == 0) return;
    Call0(bof3::addr::Magic111Flash_Draw);
}

// original 0x4D6730: once the owner's +0xB is 2 or less, +9 0 and +2 on.
C3_EXPORT void __cdecl Magic111Flash_Wait(void) {
    if (Owner()[0xB] > 2) return;
    Sc()[9] = 0;
    Inc(Sc()[2]);
}

// original 0x4D6750: tpage 0x35 (additive) as a draw mode; a
// semi-transparent flat quad over the whole screen, grey +9 x 15 (a byte);
// then tpage 0x15 as a draw mode.
C3_EXPORT void __cdecl Magic111Flash_Draw(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyF4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    FullScreen(p, 0x14, 0x18, 0x20, 0x24, 0x2C, 0x30);
    const auto grey = static_cast<unsigned char>(Sc()[9] * 15u);
    p[4] = grey;
    p[5] = grey;
    p[6] = grey;
    MH_CALL(Gfx_CommitPrim)(3, 0x38);
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
}

void Magic111_Inject() {
    BOF3_INJECT(Magic111_Task);
    BOF3_INJECT(Magic111_Start);
    BOF3_INJECT(Magic111_Wait);
    BOF3_INJECT(Magic111Child_Task);
    BOF3_INJECT(Magic111Double_Run);
    BOF3_INJECT(Magic111Double_Begin);
    BOF3_INJECT(Magic111Double_Wait);
    BOF3_INJECT(Magic111Wash_Run);
    BOF3_INJECT(Magic111Wash_Start);
    BOF3_INJECT(Magic111Wash_Draw);
    BOF3_INJECT(Magic111Flash_Run);
    BOF3_INJECT(Magic111Flash_Wait);
    BOF3_INJECT(Magic111Flash_Draw);
}

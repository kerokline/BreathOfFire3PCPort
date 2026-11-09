// Moved here by tools/move_funcs.py from magic_s09.cpp (docs/source-layout-round.md).
#include "game/magic/pollen_mote.h"

#include <climits>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s09.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
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

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, words or dwords by function) and the
// four SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read
// again after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

// kEventBattle: now EventBattle_Id (symbols.toml). u8: not 0 in an event battle
// kCurrentEnemy: now BattleE3_CurrentEnemy (symbols.toml). unsigned char *: the enemy the per-frame loops set last
constexpr ::bof3::image::Va kAbilityId{0x904B80};        // u16: the ability id (0x32 Pollen, 0x33 Venom Breath)

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

std::int32_t SD(unsigned k) { return Long(Mem(::bof3::Img(kS.v) + k)); }

void SetSD(unsigned k, std::uint32_t v) { SetLong(Mem(::bof3::Img(kS.v) + k), static_cast<std::int32_t>(v)); }

short S16(const unsigned char* at) { return static_cast<short>(move_script::Word(at)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

std::uint32_t Key(const void* p) { return Low32(p); }

std::uint32_t U(std::int32_t v) { return static_cast<std::uint32_t>(v); }

// `imul r32, r32`: the 32-bit product, wrapped.
int Imul(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)); }

// `fild dword` then `fstp dword`: an integer as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

int Sin(int angle) { return MH_CALL(Math_Sin)(angle); }

int Cos(int angle) { return MH_CALL(Math_Cos)(angle); }

// This group's functions and other units' called by address, as the originals
// call them: in the game the jmp Inject put there (or Capcom's code), in the
// fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

// Capcom's, unnamed, in no group: turns the dx / dz pair +0xC / +0x10 of the
// task it is given by its direction byte +8 (docs/magic_s22.md).
constexpr ::bof3::image::CodeVa kTurnOffset{bof3::addr::Battle_TurnVectorC};

using TaskFn = void (__cdecl*)(unsigned char*);

void Turn(unsigned char* task) { MH_AT(TaskFn, kTurnOffset.v)(task); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// A stack table's (or a .data table's) dispatch: entry `phase`, which the
// originals do not check.
void Dispatch(const std::uint32_t* phases, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    magic_harness::Phase(phases[phase])();
}

// A primitive linked at the current task's position (+0x34 / +0x38, read at
// the call) on layer 2.
void LinkAtSprite(unsigned size) {
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(ts->x), static_cast<unsigned long>(ts->z), 2,
                                size);
}

// A draw-mode packet (tpage `tpage`, dithered) at Gfx_PacketNext.
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

}  // namespace

#define S09_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// The ability word: 0x32 (Pollen read one id down) or another (Venom Breath).
bool PollenShades() { return Word(Mem(::bof3::Img(kAbilityId.v))) == 0x32; }

// A screen-space point at radius dword 0x903850 + `r` of angle `a` from the
// task's screen point, as floats: the radius and the point read after the
// call, as the original reads them.
void PutSinX(unsigned char* at, int a, unsigned r) {
    const int v = Sin(a);
    PutFloat(at, (Imul(v, SD(r)) >> 12) + S16(Sc() + 0x2E));
}

void PutCosY(unsigned char* at, int a, unsigned r) {
    const int v = Cos(a);
    PutFloat(at, (Imul(v, SD(r)) >> 12) + S16(Sc() + 0x30));
}

}  // namespace

// original 0x4AB3F0: the kind-2 task. A three-entry stack table by +1:
// Pollen_Start, Pollen_Sounds, BattleFx_Finish.
S09_EXPORT void __cdecl Pollen_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::Pollen_Start, bof3::addr::Pollen_Sounds,
                                                 bof3::addr::BattleFx_Finish};
    Dispatch(kPhases, 3, Sc()[1], "Pollen_Task");
}

// original 0x4AB420: +0xB 0; the task to the side's centre; the owner's
// direction and x / z; +9 0, +0xB 0, +1 on; ten motes (kind 1, 4; the slots
// unchecked): +0x80 this task, +1 0, +0xB the index i, +9 Pollen_Delays[i] + 1,
// +8 and the position ours; our count +0xB up each; sound 0x100.
S09_EXPORT void __cdecl Pollen_Start(void) {
    Sc()[0xB] = 0;
    MH_CALL(MagicFx_CenterOnSide)();
    Sc()[8] = Owner()[8];
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    Sc()[9] = 0;
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
    for (unsigned i = 0; i < 10; ++i) {
        const unsigned slot = NewTask(4);
        const unsigned char delay = Mem(::bof3::Img(Pollen_Delays_addr) + i)[0];
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        tchild->owner = static_cast<std::int32_t>(Key(s));
        child[1] = 0;
        child[0xB] = static_cast<unsigned char>(i);
        child[9] = static_cast<unsigned char>(delay + 1);
        child[8] = s[8];
        tchild->x = ts->x;
        tchild->z = ts->z;
        rv::Store(tchild->height, ts->height);
        Inc(s[0xB]);
    }
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4AB520: sound 0x101 at +9 8 and 0x18, 0x102 at 0x10; +9 up, at
// 0x19 +1 on.
S09_EXPORT void __cdecl Pollen_Sounds(void) {
    const unsigned char t = Sc()[9];
    if (t == 8 || t == 0x18) MH_CALL(Sound_PlayById)(0x101);
    else if (t == 0x10) MH_CALL(Sound_PlayById)(0x102);
    unsigned char* const s = Sc();
    Inc(s[9]);
    if (s[9] == 0x19) Inc(s[1]);
}

// original 0x4AB570: the motes' kind-1 task, a jmp through
// PollenMote_TaskTable (one entry) by +1, unchecked.
S09_EXPORT void __cdecl PollenMote_Task(void) {
    static constexpr std::uint32_t kKinds[1] = {bof3::addr::PollenMote_Run};
    Dispatch(kKinds, 1, Sc()[1], "PollenMote_Task");
}

// original 0x4AB590: a call through PollenMote_Steps (three entries, the
// second group C2's HolocaustBeam_Grow) by +2; then while +2 and +0 are set
// the screen point, the fan, the ring and the dots.
S09_EXPORT void __cdecl PollenMote_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::PollenMote_Start, bof3::addr::HolocaustBeam_Grow,
                                                bof3::addr::PollenMote_Fade};
    Dispatch(kSteps, 3, Sc()[2], "PollenMote_Run");
    const unsigned char* const s = Sc();
    if (s[2] == 0 || s[0] == 0) return;
    MH_CALL(BattleActor_UpdateScreenXY)();
    Call0(bof3::addr::PollenMote_DrawFan);
    Call0(bof3::addr::PollenMote_DrawRing);
    Call0(bof3::addr::PollenMote_DrawSparks);
}

// original 0x4AB5D0: +9 down; at 0: (+0xC, +0x10) PollenMote_Offsets' pair by
// +0xB (read in place, unchecked), turned by the direction, from the field's
// kind-2 point; the first mote flags the target 0x10; +9 0x20, +0xA 2, +2 on.
S09_EXPORT void __cdecl PollenMote_Start(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    rv::Store(rv::Current<records::BattleTask>()->step_x, Long(Mem(::bof3::Img(PollenMote_Offsets_addr) + 8u * Sc()[0xB])));
    rv::Store(rv::Current<records::BattleTask>()->step_z, Long(Mem(::bof3::Img(PollenMote_Offsets_addr) + 4 + 8u * Sc()[0xB])));
    Turn(Sc());
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->x = static_cast<std::int32_t>(U(ts->step_x) + U(Field_Kind2X));
        ts->z = static_cast<std::int32_t>(U(ts->step_z) + U(Field_Kind2Z));
        if (s[0xB] == 0) MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
    }
    unsigned char* const s = Sc();
    s[9] = 0x20;
    s[0xA] = 2;
    Inc(s[2]);
}

// original 0x4AB680: +0xA up, +9 down; at 0 the owner's count down and the
// task freed.
S09_EXPORT void __cdecl PollenMote_Fade(void) {
    unsigned char* const s = Sc();
    Inc(s[0xA]);
    Dec(s[9]);
    if (s[9] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4AB6C0: radius dword 0x903850 +0xA; shades 0x903858 (+9 x 3 for
// ability 0x32, else x 5) and 0x90385C (+9); per sixteenth of a turn a
// draw-mode packet (tpage 0x35) and a semi-transparent gouraud triangle from
// the screen point to two points on the circle, linked at the task (0x34).
S09_EXPORT void __cdecl PollenMote_DrawFan(void) {
    {
        const unsigned char* const s = Sc();
        SetSD(0, s[0xA]);
        SetSD(8, PollenShades() ? s[9] * 3u : s[9] * 5u);
        SetSD(0xC, s[9]);
    }
    for (int a = 0; a < 0x1000;) {
        DrawMode(0x35);
        LinkAtSprite(0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, S16(Sc() + 0x2E));
        PutFloat(p + 0xC, S16(Sc() + 0x30));
        PutSinX(p + 0x18, a, 0);
        PutCosY(p + 0x1C, a, 0);
        a += 0x100;
        PutSinX(p + 0x28, a, 0);
        PutCosY(p + 0x2C, a, 0);
        if (PollenShades()) {
            p[4] = SB(0xC);
            p[5] = SB(8);
            p[6] = SB(8);
            p[0x14] = SB(8);
            p[0x15] = SB(8);
            p[0x16] = 1;
            p[0x24] = SB(8);
            p[0x25] = SB(8);
            p[0x26] = 1;
        } else {
            p[5] = 1;
            p[4] = SB(0xC);
            p[6] = SB(8);
            p[0x14] = SB(8);
            p[0x15] = SB(0xC);
            p[0x16] = SB(8);
            p[0x24] = SB(8);
            p[0x25] = SB(0xC);
            p[0x26] = SB(8);
        }
        LinkAtSprite(0x34);
    }
}

// original 0x4AB8E0: radii dword 0x903850 +0xA and 0x903854 +0xA x 1.5;
// shades 0x903858 (+9 x 4 for ability 0x32, else x 6) and 0x90385C (+9); per
// sixteenth of a turn a draw-mode packet (tpage 0x35) and a semi-transparent
// gouraud quad of the ring between them, the outer edge shaded, the inner 1,
// linked at the task (0x44).
S09_EXPORT void __cdecl PollenMote_DrawRing(void) {
    {
        const unsigned char* const s = Sc();
        const unsigned r = s[0xA];
        SetSD(0, r);
        SetSD(4, (r >> 1) + r);
        SetSD(8, PollenShades() ? s[9] * 4u : s[9] * 6u);
        SetSD(0xC, s[9]);
    }
    for (int a = 0; a < 0x1000;) {
        const int b = a + 0x100;
        DrawMode(0x35);
        LinkAtSprite(0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutSinX(p + 8, a, 0);
        PutCosY(p + 0xC, a, 0);
        PutSinX(p + 0x18, b, 0);
        PutCosY(p + 0x1C, b, 0);
        PutSinX(p + 0x28, a, 4);
        PutCosY(p + 0x2C, a, 4);
        PutSinX(p + 0x38, b, 4);
        PutCosY(p + 0x3C, b, 4);
        if (PollenShades()) {
            p[4] = SB(0xC);
            p[5] = SB(8);
            p[6] = SB(0xC);
            p[0x14] = SB(0xC);
            p[0x15] = SB(8);
            p[0x16] = SB(0xC);
        } else {
            p[4] = SB(8);
            p[5] = SB(0xC);
            p[6] = SB(8);
            p[0x14] = SB(8);
            p[0x15] = SB(0xC);
            p[0x16] = SB(8);
        }
        for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[k] = 1;
        LinkAtSprite(0x44);
        a = b;
    }
}

// original 0x4ABB70: radius dword 0x903850 +0xA x 3; colour 0x903858 (+9 x 6)
// and 0x90385C (+9), their order by the ability word; per sixteenth of a turn
// a draw-mode packet (tpage 0x35) and a semi-transparent one-pixel tile on the
// circle, linked at the task (0x14).
S09_EXPORT void __cdecl PollenMote_DrawSparks(void) {
    {
        const unsigned char* const s = Sc();
        SetSD(0, s[0xA] * 3u);
        SetSD(8, s[9] * 6u);
        SetSD(0xC, s[9]);
    }
    for (int a = 0; a < 0x1000; a += 0x100) {
        DrawMode(0x35);
        LinkAtSprite(0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetTile1)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutSinX(p + 8, a, 0);
        PutCosY(p + 0xC, a, 0);
        p[4] = SB(8);
        if (PollenShades()) {
            p[5] = SB(8);
            p[6] = SB(0xC);
        } else {
            p[5] = SB(0xC);
            p[6] = SB(8);
        }
        LinkAtSprite(0x14);
    }
}

void PollenMote_Inject() {
    BOF3_INJECT(Pollen_Task);
    BOF3_INJECT(Pollen_Start);
    BOF3_INJECT(Pollen_Sounds);
    BOF3_INJECT(PollenMote_Task);
    BOF3_INJECT(PollenMote_Run);
    BOF3_INJECT(PollenMote_Start);
    BOF3_INJECT(PollenMote_Fade);
    BOF3_INJECT(PollenMote_DrawFan);
    BOF3_INJECT(PollenMote_DrawRing);
    BOF3_INJECT(PollenMote_DrawSparks);
}

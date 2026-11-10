// Moved here by tools/move_funcs.py from magic_c2.cpp (docs/source-layout-round.md).
#include "game/magic/rotten_breath_mote.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
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

namespace addr = bof3::addr;

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, words by function) and the first two
// words of Prim_VertexScratch (0x9037A0). Both are read again after every
// call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

// Row 26 of Gfx_ClutStrip and of its source, 0x4000 bytes below.
// kClutRow26: now Gfx_ClutStrip[0x1a00] (symbols.toml).
// kClutRow26Source: now Gfx_ClutStripSource[0x1a00] (symbols.toml).
// The overlays' own .data and .bss.
// kShakeCounts: now BoneDanceShake_Counts (symbols.toml). BoneDanceShake_Counts: four bytes by +9
constexpr ::bof3::image::Va kRottenTypes{0x65B358};      // RottenBreathChild_Types: two entries by +1

constexpr ::bof3::image::Va kMotes{0x68E1B8};            // RottenBreath_Motes: 48 records of 0x84

constexpr std::uint32_t kRecord = 0x84;

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

// [[0x93B8C4] + 0x80]: the current slot's owner - the caster's sprite.
unsigned char* Caster() {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(rv::As<records::BattleTask>(Pointer(::bof3::Img(BattleFlow_TaskCurrent_addr)))->owner))));
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

std::uint32_t Key(const void* p) { return Low32(p); }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

int SS(unsigned k) { return static_cast<short>(SW(k)); }

void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

std::int32_t Add(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}

void AddLong(unsigned char* at, std::int32_t v) { SetLong(at, Add(Long(at), v)); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `shl n` then `sar 0xC`.
int Shl12(int v, unsigned n) { return static_cast<std::int32_t>(static_cast<std::uint32_t>(v) << n) >> 12; }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter); }

int Sin(int a) { return MH_CALL(Math_Sin)(a); }

int Cos(int a) { return MH_CALL(Math_Cos)(a); }

// This group's functions called by address, as the originals call them: in the
// game the jmp Inject put there (or Capcom's code under BOF3X_ORIGINAL), in
// the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

using FnB = unsigned char (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// A stack table's dispatch: the phase byte through its entries.
void Step(const std::uint32_t* table, unsigned entries, unsigned phase, const char* who) {
    if (phase >= entries) PastTable(who, phase, entries);
    magic_harness::Phase(table[phase])();
}

// A .data table's dispatch, read in place and unchecked.
void CallCell(std::uint32_t table, unsigned index) {
    reinterpret_cast<magic_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))))();
}

// A task's pool walk: every record with bit 0 of +0 becomes the current task,
// its +0x80 the owner, for `fn`; both put back after each. The two are read
// after the task's own phase.
void Walk(std::uint32_t pool, unsigned count, std::uint32_t fn) {
    unsigned char* const self = Sprite_Current;
    const std::int32_t owner = Long(Mem(::bof3::Img(BattleE2_Owner_addr)));
    for (unsigned i = 0; i < count; ++i) {
        unsigned char* const r = Mem(pool + i * kRecord);
        if (!(r[0] & 1)) continue;
        const std::int32_t its = Long(r + 0x80);
        Sprite_Current = r;
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), its);
        Call0(fn);
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), owner);
        Sprite_Current = self;
    }
}

// The first record of a pool without bit 0 gets it; its index, 0xFF when full.
unsigned char Alloc(std::uint32_t pool, unsigned count) {
    for (unsigned i = 0; i < count; ++i) {
        unsigned char* const r = Mem(pool + i * kRecord);
        if (r[0] & 1) continue;
        r[0] = static_cast<unsigned char>(r[0] | 1);
        return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

// The caster's animation, run with Sprite_Current its sprite and put back.
void CasterAnimation(unsigned animation) {
    unsigned char* const caster = Caster();
    unsigned char* const self = Sc();
    Sprite_Current = caster;
    MH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(animation));
    Sprite_Current = self;
}

}  // namespace

#define C2_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4BF8D0: the kind-2 task. A four-entry stack table by +1 -
// RottenBreath_Start, the engine's 0x43F430 (the caster's script to its end),
// RottenBreath_Emit, RottenBreath_End - then the walk of the 48 motes.
C2_EXPORT void __cdecl RottenBreath_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {addr::RottenBreath_Start, addr::MagicFx_WaitOwnerAnim,
                                                 addr::RottenBreath_Emit, addr::RottenBreath_End};
    Step(kPhases, 4, Sc()[1], "RottenBreath_Task");
    Walk(::bof3::Img(kMotes.v), 48, addr::RottenBreathMote_Run);
}

// original 0x4BF960: the motes' +0..+2 cleared; the task at the owner's x, z
// and height word; Gfx_ClutStripDirty 1 and row 26's first sixteen entries from
// their source with bit 15 set, then entry 0 again without it; the caster's
// animation 1; +1 on.
C2_EXPORT void __cdecl RottenBreath_Start(void) {
    for (unsigned i = 0; i < 48; ++i) {
        unsigned char* const r = Mem(::bof3::Img(kMotes.v) + i * kRecord);
        r[0] = 0;
        r[1] = 0;
        r[2] = 0;
    }
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->height_whole, rv::As<records::SpriteHead>(Owner())->height_whole);
    Gfx_ClutStripDirty = 1;
    for (unsigned i = 0; i < 16; ++i) SetWord(Mem(::bof3::Img(Gfx_ClutStrip_addr) + 0x3400 + 2 * i), Word(Mem(::bof3::Img(Gfx_ClutStripSource_addr) + 0x3400 + 2 * i)) | 0x8000u);
    SetWord(Mem(::bof3::Img(Gfx_ClutStrip_addr) + 0x3400), Word(Mem(::bof3::Img(Gfx_ClutStripSource_addr) + 0x3400)));
    CasterAnimation(1);
    Inc(Sc()[1]);
}

// original 0x4BFA20: the cloud (kind 1, parameter 0x42, +1 = 0), this task its
// owner, the slot unchecked; the caster's script ticked once (the answer
// unread); +0xB 1 (the cloud's, counted down by MagicFx_EndWithChildren); +1
// on.
C2_EXPORT void __cdecl RottenBreath_Emit(void) {
    const unsigned slot = NewTask(0x42) & 0xFF;
    unsigned char* const self = Sc();
    unsigned char* const t = TaskSlot(slot);
    records::BattleTask* const tt = rv::As<records::BattleTask>(t);
    tt->owner = static_cast<std::int32_t>(Key(self));
    t[1] = 0;
    Sprite_Current = Caster();
    MH_CALL(Sprite_ScriptTickOnce)();
    Sprite_Current = self;
    self[0xB] = 1;
    Inc(Sc()[1]);
}

// original 0x4BFA90: the caster's script ticked once (unread); with +0xB 0,
// Battle_SetTargetFlag40(target), the done flag, free.
C2_EXPORT void __cdecl RottenBreath_End(void) {
    unsigned char* const caster = Caster();
    unsigned char* const self = Sc();
    Sprite_Current = caster;
    MH_CALL(Sprite_ScriptTickOnce)();
    Sprite_Current = self;
    if (self[0xB] != 0) return;
    MH_CALL(Battle_SetTargetFlag40)(Mem(::bof3::Img(at::kTarget))[0]);
    Mem(::bof3::Img(magic_harness::at::kFlags))[0] = static_cast<unsigned char>(Mem(::bof3::Img(magic_harness::at::kFlags))[0] | 4);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4BFAE0: the child (kind 1, parameter 0x42): a jmp through
// RottenBreathChild_Types by +1, read in place, unchecked - the cloud (0) or
// the aim (1).
C2_EXPORT void __cdecl RottenBreathChild_Task(void) { CallCell(::bof3::Img(kRottenTypes.v), Sc()[1]); }

// original 0x4BFB00: the cloud. A three-entry stack table by +2:
// RottenBreathCloud_Start, RottenBreathCloud_Emit, MagicFx_EndWithChildren.
C2_EXPORT void __cdecl RottenBreathCloud_Run(void) {
    static constexpr std::uint32_t kPhases[3] = {addr::RottenBreathCloud_Start, addr::RottenBreathCloud_Emit,
                                                 addr::MagicFx_EndWithChildren};
    Step(kPhases, 3, Sc()[2], "RottenBreathCloud_Run");
}

// original 0x4BFB30: the cloud two units past the owner in x, its z and
// height word; the aim (kind 1, parameter 0x42, +1 = 1) with the cloud its
// owner, its slot in +0xA, unchecked; sound 0x100; +0xB and +9 0; +2 on.
C2_EXPORT void __cdecl RottenBreathCloud_Start(void) {
    rv::Store(rv::Current<records::BattleTask>()->x, Add(rv::As<records::SpriteHead>(Owner())->x, 0x20000));
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->height_whole, rv::As<records::SpriteHead>(Owner())->height_whole);
    const unsigned slot = NewTask(0x42) & 0xFF;
    unsigned char* const self = Sc();
    unsigned char* const t = TaskSlot(slot);
    records::BattleTask* const tt = rv::As<records::BattleTask>(t);
    tt->owner = static_cast<std::int32_t>(Key(self));
    t[1] = 1;
    self[0xA] = static_cast<unsigned char>(slot);
    MH_CALL(Sound_PlayById)(0x100);
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    Inc(Sc()[2]);
}

// original 0x4BFBD0: every fourth frame a mote (RottenBreathMote_Alloc,
// unchecked) owned by the cloud, its +0xB the cloud's age +9, its +0xA 8 (age
// below 100) or 8 - (age - 100) / 4, counted in the cloud's +0xB; the age up;
// past 0x80 +2 on.
C2_EXPORT void __cdecl RottenBreathCloud_Emit(void) {
    if ((Frame_Counter & 3) == 3) {
        const unsigned m = MH_AT(FnB, addr::RottenBreathMote_Alloc)() & 0xFFu;
        unsigned char* const self = Sc();
        unsigned char* const r = Mem(::bof3::Img(kMotes.v) + m * kRecord);
        SetLong(r + 0x80, static_cast<std::int32_t>(Key(self)));
        r[0xB] = self[9];
        const unsigned age = self[9];
        r[0xA] = age < 0x64 ? 8 : static_cast<unsigned char>(8 - ((age - 0x64) >> 2));
        Inc(self[0xB]);
    }
    Inc(Sc()[9]);
    if (Sc()[9] > 0x80) Inc(Sc()[2]);
}

// original 0x4BFC60: the aim: freed once the effect is done, else each frame on
// the target side's centre (MagicFx_CenterOnSide, a tail jmp).
C2_EXPORT void __cdecl RottenBreathAim_Run(void) {
    if (Mem(::bof3::Img(at::kFlags))[0] & 4)
        MH_CALL(BattleTask_FreeCurrent)();
    else
        MH_CALL(MagicFx_CenterOnSide)();
}

// original 0x4BFC80: a mote. A four-entry stack table by +2:
// RottenBreathMote_Start, _Swirl, _Fly, _End.
C2_EXPORT void __cdecl RottenBreathMote_Run(void) {
    static constexpr std::uint32_t kPhases[4] = {addr::RottenBreathMote_Start, addr::RottenBreathMote_Swirl,
                                                 addr::RottenBreathMote_Fly, addr::RottenBreathMote_End};
    Step(kPhases, 4, Sc()[2], "RottenBreathMote_Run");
}

// original 0x4BFCC0: the mote at the cloud (x + 0x2000, z, height word +
// 0x100); its heading +0x14 Math_Ratan2(dx, dz) toward the aim, the task slot
// the cloud's +0xA names (unchecked); +9 0; +2 on.
C2_EXPORT void __cdecl RottenBreathMote_Start(void) {
    rv::Store(rv::Current<records::BattleTask>()->x, Add(rv::As<records::SpriteHead>(Owner())->x, 0x2000));
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->height_whole, rv::As<records::SpriteHead>(Owner())->height_whole + 0x100u);
    const unsigned char* const aim = TaskSlot(Owner()[0xA]);
    const records::BattleTask* const taim = rv::AsConst<records::BattleTask>(aim);
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    const std::int32_t dz = Add(taim->z, -ts->z);
    const std::int32_t dx = Add(taim->x, -ts->x);
    const int heading = MH_CALL(Math_Ratan2)(static_cast<float>(dx), static_cast<float>(dz));
    rv::Store(rv::Current<records::BattleTask>()->step_y, heading);
    Sc()[9] = 0;
    Inc(Sc()[2]);
}

// original 0x4BFD70: the swirl: the scratch word 0x903852 the angle ((+0xB +
// +9) & 0x3F) << 6; the direction +0xC the heading plus sin(it) x 300 >> 12;
// x and z moved by 7 sin / 7 cos of the direction (the low 20 bits, sign
// extended; each to the mote read before its call); the screen point; the
// draw; +9 up, at 0x10 +0xA 0x40 and +2 on.
C2_EXPORT void __cdecl RottenBreathMote_Swirl(void) {
    const unsigned char* s = Sc();
    const unsigned angle = ((static_cast<unsigned>(s[0xB]) + s[9]) & 0x3F) << 6;
    SetSW(2, angle);
    int v = Sin(static_cast<short>(angle));
    s = Sc();
    rv::Store(rv::Current<records::BattleTask>()->step_x, Add(static_cast<std::int32_t>(static_cast<std::uint32_t>(v) * 300u) >> 12, rv::As<records::BattleTask>(s)->step_y));
    unsigned char* const px = Sc() + 0x34;
    v = Sin(rv::Current<records::BattleTask>()->step_x);
    AddLong(px, Shl12(static_cast<int>(static_cast<std::uint32_t>(v) * 7u), 12));
    unsigned char* const pz = Sc() + 0x38;
    v = Cos(rv::Current<records::BattleTask>()->step_x);
    AddLong(pz, Shl12(static_cast<int>(static_cast<std::uint32_t>(v) * 7u), 12));
    MH_CALL(BattleActor_UpdateScreenXY)();
    Call0(addr::RottenBreathMote_Draw);
    Inc(Sc()[9]);
    if (Sc()[9] == 0x10) {
        Sc()[0xA] = 0x40;
        Inc(Sc()[2]);
    }
}

// original 0x4BFE40: the flight: MagicFx_StepToward(the aim's slot, 3), the
// screen point, the draw; +0xA down, at 0 +2 on.
C2_EXPORT void __cdecl RottenBreathMote_Fly(void) {
    const unsigned char* const aim = TaskSlot(Owner()[0xA]);
    MH_CALL(MagicFx_StepToward)(aim, 3);
    MH_CALL(BattleActor_UpdateScreenXY)();
    Call0(addr::RottenBreathMote_Draw);
    Dec(Sc()[0xA]);
    if (Sc()[0xA] == 0) Inc(Sc()[2]);
}

// original 0x4BFE90: drawn once more; the cloud's count down; +0..+4 cleared
// (the record free).
C2_EXPORT void __cdecl RottenBreathMote_End(void) {
    MH_CALL(BattleActor_UpdateScreenXY)();
    Call0(addr::RottenBreathMote_Draw);
    Dec(Owner()[0xB]);
    for (unsigned k = 0; k < 5; ++k) Sc()[k] = 0;
}

// original 0x4BFEE0: one semi-transparent textured quad (Gpu_SetPolyGT4) round
// the mote's screen point, half-size +9 x 8 + 8 below 8 and 0x40 after; its
// corners at angles 0xC00, 0x800, 0, 0x400; page (0x340, 0x100) mode 2, CLUT
// (0, 0x1FA), uv (1, 1) .. (0x1F, 0x1F); shade 0x80; tpage 0x55; linked at the
// mote's x, z (sizes 0xC and 0x54). The scratch words 0x903850..0x903859 hold
// the size, the screen point and the shade.
C2_EXPORT void __cdecl RottenBreathMote_Draw(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x55, 0);
    {
        const unsigned char* const s = Sc();
        const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
        MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(ts->x), static_cast<unsigned long>(ts->z), 2,
                                    0xC);
    }
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyGT4)(p);
    {
        const unsigned char* const s = Sc();
        const unsigned n = s[9];
        SetSW(0, n < 8 ? n * 8 + 8 : 0x40);
        SetSW(8, 0x80);
        SetSW(4, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x));
        SetSW(6, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_y));
    }
    static constexpr int kAngles[4] = {0xC00, 0x800, 0, 0x400};
    static constexpr unsigned kAt[4] = {8, 0x1C, 0x30, 0x44};
    for (unsigned c = 0; c < 4; ++c) {
        int v = Sin(kAngles[c]);
        PutFloat(p + kAt[c], Add(Mul12(v, SS(0)), SS(4)));
        v = Cos(kAngles[c]);
        PutFloat(p + kAt[c] + 4, Add(Mul12(v, SS(0)), SS(6)));
    }
    rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(p)->tpage, MH_CALL(Gpu_GetTPage)(0, 2, 0x340, 0x100));
    rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(p)->clut, MH_CALL(Gpu_GetClut)(0, 0x1FA));
    p[0x14] = 1;
    p[0x15] = 1;
    p[0x28] = 0x1F;
    p[0x29] = 1;
    p[0x3C] = 1;
    p[0x3D] = 0x1F;
    p[0x50] = 0x1F;
    p[0x51] = 0x1F;
    static constexpr unsigned kShade[12] = {4, 5, 6, 0x18, 0x19, 0x1A, 0x2C, 0x2D, 0x2E, 0x40, 0x41, 0x42};
    for (const unsigned k : kShade) p[k] = Mem(::bof3::Img(kS.v) + 8)[0];
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(ts->x), static_cast<unsigned long>(ts->z), 2,
                                0x54);
}

// original 0x4C0190: the first of the 48 motes without bit 0 of +0 gets it; its
// index in al, 0xFF when all are taken.
C2_EXPORT unsigned char __cdecl RottenBreathMote_Alloc(void) { return Alloc(::bof3::Img(kMotes.v), 48); }

void RottenBreathMote_Inject() {
    BOF3_INJECT(RottenBreath_Task);
    BOF3_INJECT(RottenBreath_Start);
    BOF3_INJECT(RottenBreath_Emit);
    BOF3_INJECT(RottenBreath_End);
    BOF3_INJECT(RottenBreathChild_Task);
    BOF3_INJECT(RottenBreathCloud_Run);
    BOF3_INJECT(RottenBreathCloud_Start);
    BOF3_INJECT(RottenBreathCloud_Emit);
    BOF3_INJECT(RottenBreathAim_Run);
    BOF3_INJECT(RottenBreathMote_Run);
    BOF3_INJECT(RottenBreathMote_Start);
    BOF3_INJECT(RottenBreathMote_Swirl);
    BOF3_INJECT(RottenBreathMote_Fly);
    BOF3_INJECT(RottenBreathMote_End);
    BOF3_INJECT(RottenBreathMote_Draw);
    BOF3_INJECT(RottenBreathMote_Alloc);
}

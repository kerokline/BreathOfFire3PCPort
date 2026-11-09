// Moved here by tools/move_funcs.py from magic_s36.cpp (docs/source-layout-round.md).
#include "game/magic/aura_breath_dome.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s36.h"
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

using move_script::SetWord;

using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, Scratch_Swap at +0xC) and the four
// SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again
// after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned ActorIndex() { return static_cast<unsigned>(Long(Mem(::bof3::Img(at::kActor)))) & 0xFF; }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

std::uint32_t Key(const void* p) { return Low32(p); }

std::int32_t I32(std::uint32_t v) { return static_cast<std::int32_t>(v); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `add` of two dwords, wrapping.
int Add(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b)); }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

// The originals index the enemy records by the battle index - 3, unchecked.
unsigned char* EnemyRecord(unsigned battle_index) {
    return Mem(::bof3::Img(Battle_Enemies_addr) + static_cast<std::uint32_t>(static_cast<int>(battle_index) - 3) * at::kEnemyStride);
}

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

using ReachFn = int (__cdecl*)(unsigned char*);

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// Gte_RotTransPers4 with the depth and flag pointers the original passes (ours
// reads the first).
using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);

#define S36_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

// The same, each word with its semi-transparency bit set.
void RestoreRow26Stp() {
    for (unsigned k = 0x1A00; k < 0x1B00; ++k) Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
    Gfx_ClutStripDirty = 1;
}

}  // namespace

#define S36_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4F52F0: the kind-2 task. A two-entry stack table by +1:
// AuraBreath_Start, BattleFx_Finish.
S36_EXPORT void __cdecl AuraBreath_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::AuraBreath_Start, bof3::addr::BattleFx_Finish};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("AuraBreath_Task", phase, 2);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4F5320: AuraBreath_Struck cleared; the owner's direction and
// point; +0xB 0, +1 on; the dome (kind 1, 0x67, +1 0, +9 1) with +0x80 this
// task, counted in +0xB; row 26 of the CLUT strip back with its STP bits;
// sound 0x100.
S36_EXPORT void __cdecl AuraBreath_Start(void) {
    SetLong(Mem(::bof3::Img(AuraBreath_Struck_addr)), 0);
    SetLong(Mem(::bof3::Img(AuraBreath_Struck_addr) + 4), 0);
    Sc()[8] = Owner()[8];
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
    const unsigned slot = NewTask(0x67);
    unsigned char* const s = Sc();
    unsigned char* const child = TaskSlot(slot);
    records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
    tchild->owner = I32(Key(s));
    child[1] = 0;
    child[9] = 1;
    Inc(s[0xB]);
    RestoreRow26Stp();
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4F5400: the dome's kind-1 task, a jmp through
// AuraBreathDome_TaskTable (one entry) by +1, unchecked.
S36_EXPORT void __cdecl AuraBreathDome_Task(void) {
    const unsigned phase = Sc()[1];
    if (phase >= 1) PastTable("AuraBreathDome_Task", phase, 1);
    magic_harness::Phase(bof3::addr::AuraBreathDome_Run)();
}

// original 0x4F5420: a call through AuraBreathDome_Steps (three entries:
// _Wait, _Grow, _Fade) by +2; then while +0 and +2 are set the actor matrix,
// the dome, the matrix popped.
S36_EXPORT void __cdecl AuraBreathDome_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::AuraBreathDome_Wait, bof3::addr::AuraBreathDome_Grow,
                                                bof3::addr::AuraBreathDome_Fade};
    const unsigned phase = Sc()[2];
    if (phase >= 3) PastTable("AuraBreathDome_Run", phase, 3);
    magic_harness::Phase(kSteps[phase])();
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::AuraBreathDome_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4F5460: +9 down; at 0 the owner's point, +0x5D 0x10, +9 0x10,
// +0xA 4, +2 on.
S36_EXPORT void __cdecl AuraBreathDome_Wait(void) {
    unsigned char* const s = Sc();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    Dec(s[9]);
    if (s[9] != 0) return;
    const unsigned char* const o = Owner();
    ts->x = rv::As<records::SpriteHead>(o)->x;
    ts->z = rv::As<records::SpriteHead>(o)->z;
    rv::Store(ts->height, rv::As<records::SpriteHead>(o)->height);
    s[0x5D] = 0x10;
    s[9] = 0x10;
    s[0xA] = 4;
    Inc(s[2]);
}

// original 0x4F54D0: +9 up by 2, the reach word 0x903850 +9 x 16; each enemy
// (3..10) not yet struck, not out, within reach (AuraBreath_InReach of its
// record) and not the acting actor: struck (0xFF) and flagged 0x10. On odd
// frames +0xA up below 0x20; at +9 0x60 +2 on.
S36_EXPORT void __cdecl AuraBreathDome_Grow(void) {
    AddB(Sc()[9], 2);
    SetSW(0, static_cast<unsigned>(Sc()[9]) << 4);
    for (unsigned i = 0; i < 8; ++i) {
        unsigned char* const struck = Mem(::bof3::Img(AuraBreath_Struck_addr) + i);
        if (struck[0] != 0) continue;
        if (MH_CALL(Battle_ActorIsOut)(3 + i) != 0) continue;
        if (MH_AT(ReachFn, bof3::addr::AuraBreath_InReach)(EnemyRecord(3 + i)) == 0) continue;
        if (ActorIndex() == 3 + i) continue;
        struck[0] = 0xFF;
        MH_CALL(Battle_SetTargetFlags)(3 + i, 0x10);
    }
    if (static_cast<unsigned char>(Frame_Counter) & 1) {
        unsigned char* const s = Sc();
        if (s[0xA] < 0x20) Inc(s[0xA]);
    }
    unsigned char* const s = Sc();
    if (s[9] == 0x60) Inc(s[2]);
}

// original 0x4F5590: on odd frames +0xA up below 0x20; +9 up; +0x5D down, at
// 0 the owner's +0xB down and the task freed.
S36_EXPORT void __cdecl AuraBreathDome_Fade(void) {
    const bool odd = (static_cast<unsigned char>(Frame_Counter) & 1) != 0;
    unsigned char* const s = Sc();
    if (odd && s[0xA] < 0x20) Inc(s[0xA]);
    Inc(s[9]);
    Dec(s[0x5D]);
    if (s[0x5D] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4F55E0: the dome, under the actor matrix: 8 bands (ring radius
// +9 x 16 stepping in by 0x40, height -(sin(0x100 (band + 1)) x +0xA x 16))
// of 32 semi-transparent textured quads (tpage 0x340 / 0x100, CLUT row 0x1FA,
// u by the quad's parity and the band's half, v and height from
// AuraBreathDome_V / _Height), each after a draw-mode packet (0xB5), linked
// at its corner's map point, shaded +0x5D x 8 (a signed byte).
S36_EXPORT void __cdecl AuraBreathDome_Draw(void) {
    {
        const unsigned char* const s = Sc();
        const unsigned shade = static_cast<unsigned>(static_cast<int>(static_cast<signed char>(s[0x5D])) << 3);
        SetSW(8, shade);
        SetSW(0xA, shade);
        SetSW(0xC, shade);
        SetSW(0, static_cast<unsigned>(s[0xA]) << 4);
        SetSW(2, static_cast<unsigned>(s[9]) << 4);
    }
    int v = MH_CALL(Math_Sin)(0);
    SetVW(0xC, 0u - static_cast<unsigned>(Mul12(v, SS(0))));
    int angle = 0x100;
    for (unsigned band = 0; band < 8; ++band, angle += 0x100) {
        const unsigned radius = SW(2);
        SetSW(4, radius);
        SetSW(2, radius - 0x40);
        v = MH_CALL(Math_Sin)(0);
        SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(4))));
        v = MH_CALL(Math_Cos)(0);
        SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(4))));
        {
            const std::uint16_t z = VW(0xC);
            SetVW(0x14, z);
            SetVW(0x1C, z);
        }
        v = MH_CALL(Math_Sin)(0);
        SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
        v = MH_CALL(Math_Cos)(0);
        SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(2))));
        v = MH_CALL(Math_Sin)(angle);
        {
            const unsigned z = 0u - static_cast<unsigned>(Mul12(v, SS(0)));
            SetVW(4, z);
            SetVW(0xC, z);
        }
        const unsigned char half = static_cast<unsigned char>((band >> 2) << 1);
        unsigned char j = 1;
        do {
            const unsigned a = (j & 0x1Fu) << 7;
            SetSW(6, a);
            {
                const std::uint16_t x = VW(8), y = VW(0xA);
                SetVW(0, x);
                SetVW(2, y);
            }
            v = MH_CALL(Math_Sin)(static_cast<short>(a));
            SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
            v = MH_CALL(Math_Cos)(SS(6));
            const short next = SS(6);
            {
                const std::uint16_t x = VW(0x18);
                SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(2))));
                const std::uint16_t y = VW(0x1A);
                SetVW(0x10, x);
                SetVW(0x12, y);
            }
            v = MH_CALL(Math_Sin)(next);
            SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(4))));
            v = MH_CALL(Math_Cos)(SS(6));
            const int corner_x = static_cast<short>(VW(0x18));
            const int zv = Mul12(v, SS(4));
            const unsigned char* const s = Sc();
            const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
            SetVW(0x1A, static_cast<unsigned>(zv));
            const int px = Add(static_cast<int>(static_cast<std::uint32_t>(corner_x) << 9), ts->x);
            const int pz = Add(static_cast<int>(static_cast<std::uint32_t>(static_cast<short>(zv)) << 9), ts->z);
            MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0xB5, 0);
            MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(px), static_cast<unsigned long>(pz), 1, 0xC);
            unsigned char* const p = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyFT4)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, MH_CALL(Gpu_GetTPage)(1, 1, 0x340, 0x100) & 0xFFFF);
            rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, MH_CALL(Gpu_GetClut)(0, 0x1FA) & 0xFFFF);
            const unsigned char u = static_cast<unsigned char>(((j & 1) + half) << 6);
            const unsigned char tv = Mem(::bof3::Img(AuraBreathDome_V_addr) + band)[0];
            const unsigned char bottom = static_cast<unsigned char>(tv + Mem(::bof3::Img(AuraBreathDome_Height_addr) + band)[0] - 1);
            p[0x14] = u;
            p[0x15] = tv;
            p[0x24] = static_cast<unsigned char>(u + 0x3F);
            p[0x25] = tv;
            p[0x34] = u;
            p[0x35] = bottom;
            p[0x44] = static_cast<unsigned char>(u + 0x3F);
            p[0x45] = bottom;
            {
                long depth, flag;
                S36_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), p + 8, p + 0x18, p + 0x28, p + 0x38,
                                                 &depth, &flag);
            }
            MH_CALL(Gte_PrimDepths4_10)(p);
            p[4] = SB(8);
            p[5] = SB(0xA);
            p[6] = SB(0xC);
            MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(px), static_cast<unsigned long>(pz), 1, 0x48);
            ++j;
        } while (j < 0x21);
    }
}

// original 0x4F5970: 1 when the record's map point lies within the reach word
// 0x903850 of Sprite_Current's (the squares of the (x sar 9, z sar 9)
// differences, wrapping, against the reach squared, signed), else 0 - in all
// of eax. Also called by MAGIC058 (group S11's Sanctuary).
S36_EXPORT int __cdecl AuraBreath_InReach(unsigned char* record) {
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    const std::uint32_t sx = static_cast<std::uint32_t>(ts->x >> 9);
    const std::uint32_t sz = static_cast<std::uint32_t>(ts->z >> 9);
    const std::uint32_t rx = static_cast<std::uint32_t>(Long(record + 0x34) >> 9);
    const std::uint32_t rz = static_cast<std::uint32_t>(Long(record + 0x38) >> 9);
    const std::uint32_t dx = rx - sx, dz = rz - sz;
    const int reach = SS(0);
    const std::int32_t d2 = I32(dx * dx + dz * dz);
    const std::int32_t r2 = I32(static_cast<std::uint32_t>(reach) * static_cast<std::uint32_t>(reach));
    return r2 >= d2 ? 1 : 0;
}

void AuraBreathDome_Inject() {
    BOF3_INJECT(AuraBreath_Task);
    BOF3_INJECT(AuraBreath_Start);
    BOF3_INJECT(AuraBreathDome_Task);
    BOF3_INJECT(AuraBreathDome_Run);
    BOF3_INJECT(AuraBreathDome_Wait);
    BOF3_INJECT(AuraBreathDome_Grow);
    BOF3_INJECT(AuraBreathDome_Fade);
    BOF3_INJECT(AuraBreathDome_Draw);
    BOF3_INJECT(AuraBreath_InReach);
}

// Moved here by tools/move_funcs.py from magic_s28.cpp (docs/source-layout-round.md).
#include "game/magic/breath_beam.h"

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

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

// SharedScratch's sixteen bytes (0x903850..0x90385F), words the overlays keep
// their working values in, and Prim_VertexScratch's first two words (MAGIC124's
// bolt keeps its moving point there). All are read again after every call, as
// the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

void AddSW(unsigned k, unsigned v) { SetSW(k, SW(k) + v); }

short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }

unsigned char U8(int v) { return static_cast<unsigned char>(v); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

std::uint32_t Addr(const void* p) { return Low32(p); }

// imul r32 (the low 32 bits), shl, sar: 32-bit arithmetic as the originals do it.
std::int32_t Mul(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}

std::int32_t Shl(std::int32_t a, unsigned n) { return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) << n); }

std::int32_t Sar(std::int32_t a, unsigned n) { return a >> n; }

// `fild dword` then `fstp dword`: an integer as a float, into a primitive.
void PutF(unsigned char* prim, unsigned off, std::int32_t v) {
    const float f = static_cast<float>(v);
    std::memcpy(prim + off, &f, sizeof f);
}

int Sin(int a) { return MH_CALL(Math_Sin)(a); }

int Cos(int a) { return MH_CALL(Math_Cos)(a); }

int Ratan2(int y, int x) { return MH_CALL(Math_Ratan2)(static_cast<float>(y), static_cast<float>(x)); }

void Commit(unsigned size) { MH_CALL(Gfx_CommitPrim)(3, size); }

// The draw-mode primitive every draw of these overlays opens and closes with:
// Gpu_SetDrawMode(Gfx_PacketNext, 0, 1, tpage, 0), committed at depth 3.
void DrawMode(unsigned tpage) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
    Commit(0xC);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

void CallCell(const char* who, std::uint32_t table, unsigned index, unsigned entries) {
    if (index >= entries) PastTable(who, index, entries);
    reinterpret_cast<magic_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))))();
}

// The offset from the targets' centre toward the caster's side, as MAGIC122
// and MAGIC123 place their effect: the party's own formation offset for a
// party caster, else a fixed dx / dy (+0xC / +0x10) by the direction byte +8.
void OffsetByDirection() {
    if (Mem(::bof3::Img(at::kActor))[0] < 3) {
        MH_CALL(MagicFx_FormationOffset)();
        return;
    }
    unsigned char* const sc = Sc();
    records::BattleTask* const tsc = rv::As<records::BattleTask>(sc);
    switch (sc[8]) {
    case 0: tsc->step_x = 0x29; tsc->step_z = -13; break;
    case 1: tsc->step_x = -41; tsc->step_z = -13; break;
    case 2: tsc->step_x = -38; tsc->step_z = -38; break;
    default: tsc->step_x = 0x26; tsc->step_z = -38; break;
    }
}

// The caster's position (+0x34, +0x38, +0x3C from the owner), each dword with
// the task and the owner read again.
void CopyOwnerPosition() {
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
}

// The beam's half-width at a step: v (a word) divided by the task's +0xB, plus
// one. The original's idiv faults on a zero +0xB; ours aborts.
unsigned BeamWidth(int v) {
    SetSW(0, static_cast<unsigned>(v));
    const unsigned d = Sc()[0xB];
    if (d == 0) bof3::Fatal("BreathBeam: +0xB is 0 (the original divides by zero)");
    const int q = static_cast<short>(v) / static_cast<int>(d) + 1;
    SetSW(0, static_cast<unsigned>(q));
    return static_cast<unsigned>(q);
}

}  // namespace

#define S28_EXPORT extern "C" __attribute__((disable_tail_calls))

// The two band draws share their walk: from the caster's screen point
// (+0x2E / +0x30) toward +0x14, step i = 1..+0xA moves the point (scratch +8 /
// +0xA, the previous one kept in +0xC / +0xE) 16 units along an angle that
// swings by the sine of (+9 + i); each step draws two quads, one either side
// of the path, whose outer edge is a half-width (BeamWidth) from it.
struct BeamStep {
    int i;
    int ang_a, ang_b;    // the two widths' phases, from i - +9
};

BeamStep BeamWalk(int i, unsigned mask, unsigned shift) {
    SetSW(0xC, SW(8));
    SetSW(0xE, SW(0xA));
    int s = Sin(((Sc()[9] + i) & 0x3F) << 6);
    const unsigned angle = static_cast<unsigned>(Sar(Shl(s, 8), 12) + SW(2)) & 0xFFF;
    SetSW(4, angle);
    s = Sin(static_cast<short>(angle));
    AddSW(8, static_cast<unsigned>(Sar(Shl(s, 4), 12)));
    s = Cos(SS(4));
    AddSW(0xA, static_cast<unsigned>(Sar(Shl(s, 4), 12)));
    const int d = i - static_cast<int>(Sc()[9]);
    return {i, static_cast<int>((static_cast<unsigned>(d) & mask) << shift),
            static_cast<int>((static_cast<unsigned>(d - 1) & mask) << shift)};
}

void BeamStart() {
    DrawMode(0x35);
    const unsigned char* const sc = Sc();
    SetSW(8, static_cast<std::uint16_t>(rv::As<records::BattleTask>(sc)->screen_x));
    SetSW(0xA, static_cast<std::uint16_t>(rv::As<records::BattleTask>(sc)->screen_y));
    SetSW(2, static_cast<std::uint16_t>(rv::As<records::BattleTask>(sc)->step_y));
}

// A vertex at the half-width S50 from the point (base x, base y) along S54.
void BeamVertex(unsigned char* p, unsigned off, unsigned bx, unsigned by) {
    int s = Sin(SS(4));
    PutF(p, off, Sar(Mul(s, SS(0)), 12) + SS(bx));
    s = Cos(SS(4));
    PutF(p, off + 4, Sar(Mul(s, SS(0)), 12) + SS(by));
}

// original 0x4DD8B0: the kind-2 task. A two-entry stack table by +1:
// Firebreath_Start, then S17's Leech_WaitOrbs (the wait for the children).
S28_EXPORT void __cdecl Firebreath_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Firebreath_Start, bof3::addr::Leech_WaitOrbs};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("Firebreath_Task", phase, 2);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4DD8E0: the task takes the owner's direction and position; one
// beam (kind 1, 0x38: +0x80 this task, +1 0, +9 4); CLUT row 26 (0x812980)
// from its buffer 0x4000 below, words 1..15 and 0x21..0x2F with bit 15 (half
// transparent), 0x11..0x1F as they are, and words 0, 0x10, 0x20 zero;
// Gfx_ClutStripDirty; +0xB and +9 0, +1 on. The task's answer is used
// unchecked.
S28_EXPORT void __cdecl Firebreath_Start(void) {
    Sc()[8] = Owner()[8];
    CopyOwnerPosition();
    const unsigned slot = MH_CALL(BattleTask_Create)(1, 0x38) & 0xFFu;
    unsigned char* const self = Sc();
    unsigned char* const child = TaskSlot(slot);
    records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
    tchild->owner = static_cast<std::int32_t>(Addr(self));
    child[1] = 0;
    child[9] = 4;
    constexpr unsigned kRow = 0x1A00;
    for (unsigned k = 1; k < 0x10; ++k) Gfx_ClutStrip[kRow + k] = static_cast<unsigned short>(Gfx_ClutStripSource[kRow + k] | 0x8000);
    for (unsigned k = 0x11; k < 0x20; ++k) Gfx_ClutStrip[kRow + k] = Gfx_ClutStripSource[kRow + k];
    for (unsigned k = 0x21; k < 0x30; ++k) Gfx_ClutStrip[kRow + k] = static_cast<unsigned short>(Gfx_ClutStripSource[kRow + k] | 0x8000);
    Gfx_ClutStrip[kRow] = 0;
    Gfx_ClutStrip[kRow + 0x10] = 0;
    Gfx_ClutStrip[kRow + 0x20] = 0;
    Gfx_ClutStripDirty = 1;
    self[0xB] = 0;
    Sc()[9] = 0;
    Inc(Sc()[1]);
}

// original 0x4DD9F0: the beam's kind-1 task (0x38), a jmp through
// BreathBeam_Phases (one entry, BreathBeam_Run) by +1.
S28_EXPORT void __cdecl BreathBeam_Task(void) { CallCell("BreathBeam_Task", ::bof3::Img(BreathBeam_Phases_addr), Sc()[1], 1); }

// original 0x4DDA10: a call through BreathBeam_Steps (five entries: Aim,
// Widen, Hit, Hold, Fade) by +2; then, unless the owner's +0xB is 0xFF or +2
// is 0, the glow band and the textured band.
S28_EXPORT void __cdecl BreathBeam_Run(void) {
    CallCell("BreathBeam_Run", ::bof3::Img(BreathBeam_Steps_addr), Sc()[2], 5);
    if (Owner()[0xB] == 0xFF) return;
    if (Sc()[2] == 0) return;
    MH_CALL(BreathBeam_DrawGlow)();
    MH_CALL(BreathBeam_DrawTextured)();
}

// original 0x4DDA50: +9 down; at 0 the beam is aimed. The direction from the
// owner; the targets' centre (MagicFx_CenterOnSide), its screen point kept in
// the scratch words +0xC / +0xE; the caster's position from the owner, its
// screen point moved by the formation or direction offset (+0xC / +0x10) into
// +0x2E / +0x30 and the scratch +8 / +0xA; +0x14 the angle Math_Ratan2 gives
// from there to the centre; +4 0x10, +0xB 0x11, +9 0, +0xA 1, +2 on; sound
// 0x100.
S28_EXPORT void __cdecl BreathBeam_Aim(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Sc()[8] = Owner()[8];
    MH_CALL(MagicFx_CenterOnSide)();
    MH_CALL(BattleActor_UpdateScreenXY)();
    SetSW(0xC, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x));
    SetSW(0xE, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y));
    CopyOwnerPosition();
    MH_CALL(BattleActor_UpdateScreenXY)();
    OffsetByDirection();
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_x, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->step_x));
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_y, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->step_z));
    const short x = S16(Sc() + 0x2E), y = S16(Sc() + 0x30);
    SetSW(8, static_cast<unsigned short>(x));
    SetSW(0xA, static_cast<unsigned short>(y));
    const int angle = Ratan2(SS(0xC) - x, SS(0xE) - y);
    rv::Store(rv::Current<records::BattleTask>()->step_y, angle);
    Sc()[4] = 0x10;
    Sc()[0xB] = 0x11;
    Sc()[9] = 0;
    Sc()[0xA] = 1;
    Inc(Sc()[2]);
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4DDC00: +0xA up by 3 and +9 up; at 8, +2 on.
S28_EXPORT void __cdecl BreathBeam_Widen(void) {
    Sc()[0xA] = U8(Sc()[0xA] + 3);
    Inc(Sc()[9]);
    if (Sc()[9] == 8) Inc(Sc()[2]);
}

// original 0x4DDC30: +0xB down, +9 up; at 0x18 the target flags 0x10 and +2 on.
S28_EXPORT void __cdecl BreathBeam_Hit(void) {
    Dec(Sc()[0xB]);
    Inc(Sc()[9]);
    if (Sc()[9] != 0x18) return;
    MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
    Inc(Sc()[2]);
}

// original 0x4DDC70: +9 up; at 0x78, +2 on.
S28_EXPORT void __cdecl BreathBeam_Hold(void) {
    Inc(Sc()[9]);
    if (Sc()[9] == 0x78) Inc(Sc()[2]);
}

// original 0x4DDC90: +0xB up, +4 (the shade) down, +9 up; at 0x88 the owner's
// +0xB 0xFF and the task freed.
S28_EXPORT void __cdecl BreathBeam_Fade(void) {
    Inc(Sc()[0xB]);
    Dec(Sc()[4]);
    Inc(Sc()[9]);
    if (Sc()[9] != 0x88) return;
    Owner()[0xB] = 0xFF;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4DDCE0: the beam's textured band. Draw mode 0x35; for each step
// two GT4 quads (0x54 bytes), u 0x40..0x80 then 0..0x40, v ((+9 x 30 + i) x 8),
// the CLUT (0, 0x1FA) and page (0, 1, 0x380, 0x100), shade +4 x 8 on the
// outer edge and +4 on the inner; half-widths ((sin << 1) >> 12) + 9i - 5 and
// + 9i + 4 over +0xB; then draw mode 0x15.
S28_EXPORT void __cdecl BreathBeam_DrawTextured(void) {
    BeamStart();
    for (int i = 1; i < Sc()[0xA] + 1; ++i) {
        const BeamStep st = BeamWalk(i, 0x3F, 6);
        for (int side = 0; side < 2; ++side) {
            unsigned char* const p = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyGT4)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            SetSW(4, (SW(2) + (side == 0 ? 0x400u : 0xFC00u)) & 0xFFF);
            const unsigned in0 = side == 0 ? 0x1C : 8, out0 = side == 0 ? 0x44 : 0x30;
            const unsigned in1 = side == 0 ? 8 : 0x1C, out1 = side == 0 ? 0x30 : 0x44;
            int s = Sin(st.ang_b);
            BeamWidth(Sar(Shl(s, 1), 12) + 9 * i - 5);
            BeamVertex(p, in0, 0xC, 0xE);
            s = Sin(st.ang_a);
            BeamWidth(Sar(Shl(s, 1), 12) + 9 * i + 4);
            BeamVertex(p, out0, 8, 0xA);
            PutF(p, in1, SS(0xC));
            PutF(p, in1 + 4, SS(0xE));
            PutF(p, out1, SS(8));
            PutF(p, out1 + 4, SS(0xA));
            rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(p)->clut, MH_CALL(Gpu_GetClut)(0, 0x1FA));
            rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(p)->tpage, MH_CALL(Gpu_GetTPage)(0, 1, 0x380, 0x100));
            p[0x14] = side == 0 ? 0x40 : 0;
            p[0x28] = side == 0 ? 0x80 : 0x40;
            p[0x15] = U8((Sc()[9] * 0x1E + i) << 3);
            p[0x29] = U8((Sc()[9] * 0x1E + i) << 3);
            p[0x3C] = side == 0 ? 0x40 : 0;
            p[0x50] = side == 0 ? 0x80 : 0x40;
            p[0x3D] = U8((Sc()[9] * 0x1E + i + 1) << 3);
            p[0x51] = U8((Sc()[9] * 0x1E + i + 1) << 3);
            // the outer pair of vertices takes +4 x 8, the inner +4 (the word
            // 0x903856 holds each while its bytes are copied)
            SetSW(6, static_cast<unsigned>(Sc()[4]) << 3);
            const unsigned char outer = U8(SW(6));
            SetSW(6, Sc()[4]);
            const unsigned char inner = U8(SW(6));
            const unsigned char first = side == 0 ? outer : inner, second = side == 0 ? inner : outer;
            // side 0: +4 / +0x2C outer, +0x18 / +0x40 inner; side 1 the other way
            p[4] = p[5] = p[6] = first;
            p[0x2C] = p[0x2D] = p[0x2E] = first;
            p[0x18] = p[0x19] = p[0x1A] = second;
            p[0x40] = p[0x41] = p[0x42] = second;
            Commit(0x54);
        }
    }
    DrawMode(0x15);
}

// original 0x4DE370: the beam's glow band. As the textured band, but G4 quads
// (0x44 bytes), phases ((i - +9) & 0xF) << 8, half-widths
// ((sin x (i - 1) x 6) >> 12) + 18i - 14 and ((sin x i x 6) >> 12) + 18i + 4
// over +0xB, shade (+4 x 8, +4 x 4, +4 x 4) on the outer edge and 1 on the
// inner.
S28_EXPORT void __cdecl BreathBeam_DrawGlow(void) {
    BeamStart();
    for (int i = 1; i < Sc()[0xA] + 1; ++i) {
        const BeamStep st = BeamWalk(i, 0xF, 8);
        for (int side = 0; side < 2; ++side) {
            unsigned char* const p = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyG4)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            SetSW(4, (SW(2) + (side == 0 ? 0x400u : 0xFC00u)) & 0xFFF);
            const unsigned in0 = side == 0 ? 0x18 : 8, out0 = side == 0 ? 0x38 : 0x28;
            const unsigned in1 = side == 0 ? 8 : 0x18, out1 = side == 0 ? 0x28 : 0x38;
            int s = Sin(st.ang_b);
            BeamWidth(Sar(Mul(Mul(s, i - 1), 6), 12) + 18 * i - 14);
            BeamVertex(p, in0, 0xC, 0xE);
            s = Sin(st.ang_a);
            BeamWidth(Sar(Mul(Mul(s, i), 6), 12) + 18 * i + 4);
            BeamVertex(p, out0, 8, 0xA);
            PutF(p, in1, SS(0xC));
            PutF(p, in1 + 4, SS(0xE));
            PutF(p, out1, SS(8));
            PutF(p, out1 + 4, SS(0xA));
            // the shaded vertices: +4 x 8 red, +4 x 4 green and blue (the word
            // 0x903856); side 0 shades +4 and +0x24, side 1 +0x14 and +0x34
            SetSW(6, static_cast<unsigned>(Sc()[4]) << 2);
            const unsigned lit = side == 0 ? 4 : 0x14, lit2 = side == 0 ? 0x24 : 0x34;
            const unsigned dark = side == 0 ? 0x14 : 4, dark2 = side == 0 ? 0x34 : 0x24;
            p[lit] = U8(Sc()[4] << 3);
            p[lit + 1] = U8(SW(6));
            p[lit + 2] = U8(SW(6));
            p[dark] = p[dark + 1] = p[dark + 2] = 1;
            p[lit2] = U8(Sc()[4] << 3);
            p[lit2 + 1] = U8(SW(6));
            p[lit2 + 2] = U8(SW(6));
            p[dark2] = p[dark2 + 1] = p[dark2 + 2] = 1;
            Commit(0x44);
        }
    }
    DrawMode(0x15);
}

void BreathBeam_Inject() {
    BOF3_INJECT(Firebreath_Task);
    BOF3_INJECT(Firebreath_Start);
    BOF3_INJECT(BreathBeam_Task);
    BOF3_INJECT(BreathBeam_Run);
    BOF3_INJECT(BreathBeam_Aim);
    BOF3_INJECT(BreathBeam_Widen);
    BOF3_INJECT(BreathBeam_Hit);
    BOF3_INJECT(BreathBeam_Hold);
    BOF3_INJECT(BreathBeam_Fade);
    BOF3_INJECT(BreathBeam_DrawTextured);
    BOF3_INJECT(BreathBeam_DrawGlow);
}

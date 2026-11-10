// Moved here by tools/move_funcs.py from magic_s03.cpp (docs/source-layout-round.md).
#include "game/magic/mind_sword_blade.h"

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

using move_script::SetWord;

using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, words or dwords by function) and the
// SVECTORs of Prim_VertexScratch (0x9037A0..). Both are read again after every
// call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char* Src() { return Pointer(::bof3::Img(BattleOdds_ActorObject_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

short S16(const unsigned char* at) { return static_cast<short>(move_script::Word(at)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

std::uint32_t Key(const void* p) { return Low32(p); }

// A dword a + b (or a - b) as the originals' add / sub: wrapping.
std::int32_t Add(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}

std::int32_t Sub(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b));
}

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `shl n` then `sar m` on a dword.
int ShlSar(std::uint32_t v, unsigned n, unsigned m) { return static_cast<int>(v << n) >> m; }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

float AsFloat(std::int32_t v) { return static_cast<float>(v); }

// Gte_RotTransPers3 with the depth and flag pointers the original pushes (the
// prototype takes the first; cdecl, the caller pops the eighth).
using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, unsigned char*, unsigned char*, unsigned char*,
                               long*, long*);

#define S03_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

void DrawModeCommit(unsigned tpage, unsigned slot) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
    MH_CALL(Gfx_CommitPrim)(slot, 0xC);
}

void LinkAtSprite(int dy, unsigned size) {
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(ts->x), static_cast<unsigned long>(ts->z), dy,
                                size);
}

// A vertex of Mind Sword's blades: the angle word 0x903858 written, then the
// float x / y at prim + at / + at + 4 of the centre words 0x903850 / 0x903852
// plus sin / cos x the radius word at 0x903850 + radius, each read back after
// its call (the cosine's angle too).
void AngleVertex(unsigned char* p, unsigned at_, unsigned angle, unsigned radius) {
    SetSW(8, angle);
    int v = MH_CALL(Math_Sin)(static_cast<short>(angle));
    PutFloat(p + at_, Add(Mul12(v, SS(radius)), SS(0)));
    v = MH_CALL(Math_Cos)(SS(8));
    PutFloat(p + at_ + 4, Add(Mul12(v, SS(radius)), SS(2)));
}

unsigned BladeAngle(unsigned add) { return (Word(Sc() + 0x14) + add) & 0xFFFu; }

// The four vertices' colours of a gouraud quad (+4, +0x14, +0x24, +0x34).
struct Shades { unsigned char c[4][3]; };

void PutShades(unsigned char* p, const Shades& s) {
    for (unsigned k = 0; k < 4; ++k)
        for (unsigned c = 0; c < 3; ++c) p[4 + 0x10 * k + c] = s.c[k][c];
}

// One of a blade's two quads: radius 0x40 at +0x200, 0x38 at +a1, the centre,
// the radius word 0x903854 at +a3 (the dead store of 0x400 to the angle word
// before it, where the original has one).
void BladeQuad(unsigned a1, unsigned a3, bool store_400, const Shades& shades) {
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyG4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    SetSW(6, 0x40);
    AngleVertex(p, 8, BladeAngle(0x200), 6);
    SetSW(6, 0x38);
    AngleVertex(p, 0x18, BladeAngle(a1), 6);
    PutFloat(p + 0x28, SS(0));
    PutFloat(p + 0x2C, SS(2));
    if (store_400) SetSW(8, 0x400);
    AngleVertex(p, 0x38, BladeAngle(a3), 4);
    PutShades(p, shades);
    MH_CALL(Gfx_CommitPrim)(3, 0x44);
}

// The two quads with their shades, between which the originals differ.
void BladePair(const Shades& first, const Shades& second) {
    BladeQuad(0x230, 0x400, true, first);
    BladeQuad(0x1D0, 0, false, second);
}

}  // namespace

#define S03_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x49C3D0: the kind-2 task. A two-entry stack table by +1:
// MindSword_Start, BattleFx_Finish.
S03_EXPORT void __cdecl MindSword_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::MindSword_Start, bof3::addr::BattleFx_Finish};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("MindSword_Task", phase, 2);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x49C400: the owner's direction and position; +0xB and +9 0, +1
// on; four blades (kind 1, 0x53): +0x80 this task, +1 0, +0xB i, +9 i + 1,
// this task's +0xB counting them.
S03_EXPORT void __cdecl MindSword_Start(void) {
    Sc()[8] = Owner()[8];
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    Inc(Sc()[1]);
    for (unsigned i = 0; i < 4; ++i) {
        const unsigned slot = NewTask(0x53);
        unsigned char* const s = Sc();
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        tchild->owner = static_cast<std::int32_t>(Key(s));
        child[1] = 0;
        child[0xB] = static_cast<unsigned char>(i);
        child[9] = static_cast<unsigned char>(i + 1);
        Inc(s[0xB]);
    }
}

// original 0x49C4C0: the kind-1 task, a jmp through MindSwordChild_Kinds
// (three entries) by +1, unchecked.
S03_EXPORT void __cdecl MindSwordChild_Task(void) {
    static constexpr std::uint32_t kKinds[3] = {bof3::addr::MindSwordBlade_Run, bof3::addr::MindSwordSpark_Run,
                                                bof3::addr::MindSwordFlash_Run};
    const unsigned phase = Sc()[1];
    if (phase >= 3) PastTable("MindSwordChild_Task", phase, 3);
    magic_harness::Phase(kKinds[phase])();
}

// original 0x49C4E0: a six-entry stack table by +2 (_Appear, _Spin, _Wait,
// _Fly, _Burst, MAGIC060's 0x4B1740); then while +0 and +2 are set the screen
// point, and the lead blade's draw for +0xB 0, the others' for the rest.
S03_EXPORT void __cdecl MindSwordBlade_Run(void) {
    static constexpr std::uint32_t kSteps[6] = {
        bof3::addr::MindSwordBlade_Appear, bof3::addr::MindSwordBlade_Spin,  bof3::addr::MindSwordBlade_Wait,
        bof3::addr::MindSwordBlade_Fly,    bof3::addr::MindSwordBlade_Burst, bof3::addr::MagicFx_CountDownRelease};
    const unsigned phase = Sc()[2];
    if (phase >= 6) PastTable("MindSwordBlade_Run", phase, 6);
    magic_harness::Phase(kSteps[phase])();
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(BattleActor_UpdateScreenXY)();
    if (Sc()[0xB] != 0) Call0(bof3::addr::MindSwordBlade_Draw);
    else Call0(bof3::addr::MindSwordBlade_DrawLead);
}

// original 0x49C560: +9 down; at 0 the blade over the owner (its height +
// 0x800000), the screen point, the heading to the source sprite in the plane
// (+0xC; +0x10 0) and on screen less 0x200 (+0x14, mod 0x1000); sound 0x100;
// +9 0x20, +2 on.
S03_EXPORT void __cdecl MindSwordBlade_Appear(void) {
    Dec(Sc()[9]);
    unsigned char* s = Sc();
    if (s[9] != 0) return;
    rv::Store(rv::As<records::BattleTask>(s)->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, Add(rv::As<records::SpriteHead>(Owner())->height, 0x800000));
    MH_CALL(BattleActor_UpdateScreenXY)();
    unsigned char* const src = Src();
    s = Sc();
    {
        const std::int32_t dz = Sub(rv::As<records::SpriteHead>(src)->z, rv::As<records::BattleTask>(s)->z);
        const std::int32_t dx = Sub(rv::As<records::SpriteHead>(src)->x, rv::As<records::BattleTask>(s)->x);
        const int heading = MH_CALL(Math_Ratan2)(AsFloat(dx), AsFloat(dz));
        rv::Current<records::BattleTask>()->step_x = heading;
    }
    rv::Current<records::BattleTask>()->step_z = 0;
    s = Sc();
    {
        const int dy = S16(src + 0x30) - S16(s + 0x30);
        const int dx = S16(src + 0x2E) - S16(s + 0x2E);
        const int angle = MH_CALL(Math_Ratan2)(AsFloat(dx), AsFloat(dy));
        rv::Store(rv::Current<records::BattleTask>()->step_y, angle);
    }
    s = Sc();
    rv::Store(rv::As<records::BattleTask>(s)->step_y, Sub(rv::As<records::BattleTask>(s)->step_y, 0x200) & 0xFFF);
    MH_CALL(Sound_PlayById)(0x100);
    Sc()[9] = 0x20;
    Inc(Sc()[2]);
}

// original 0x49C680: the screen angle +0x14 on by 0x80; +9 down; at 0 +9 =
// +0xB + 12 and +2 on.
S03_EXPORT void __cdecl MindSwordBlade_Spin(void) {
    unsigned char* const s = Sc();
    rv::Store(rv::As<records::BattleTask>(s)->step_y, Add(rv::As<records::BattleTask>(s)->step_y, 0x80));
    Dec(s[9]);
    if (s[9] != 0) return;
    s[9] = static_cast<unsigned char>(s[0xB] + 0xC);
    Inc(s[2]);
}

// original 0x49C6C0: +9 down; at 0 sound 0x101 and +2 on.
S03_EXPORT void __cdecl MindSwordBlade_Wait(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Sound_PlayById)(0x101);
    Inc(Sc()[2]);
}

// original 0x49C6F0: a step (speed 0x60) toward the source sprite's point at
// half its height + 0x800000; the heading kept in +0x10, a new one to the
// source in +0xC. At the source (MagicFx_NearSprite, 0x20000), or once the
// heading has turned by more than 0x600 and less than 0xA00: the lead blade
// (+0xB 0) flags the target 0x10, and +2 on.
S03_EXPORT void __cdecl MindSwordBlade_Fly(void) {
    unsigned char* const src = Src();
    {
        const unsigned x = static_cast<unsigned>((rv::As<records::SpriteHead>(src)->x >> 9) - 0x4000);
        const unsigned z = static_cast<unsigned>((rv::As<records::SpriteHead>(src)->z >> 9) - 0x4000);
        const unsigned y = static_cast<unsigned>(Add(rv::As<records::SpriteHead>(src)->height, 0x800000) >> 17);
        // the fourth word is the original's uninitialised stack; the callee reads none of it
        MH_CALL(MagicFx_StepTowardPoint)(x, z, y, 0, 0x60);
    }
    unsigned char* s = Sc();
    rv::Store(rv::As<records::BattleTask>(s)->step_z, rv::As<records::BattleTask>(s)->step_x);
    s = Sc();
    {
        const std::int32_t dz = Sub(rv::As<records::SpriteHead>(src)->z, rv::As<records::BattleTask>(s)->z);
        const std::int32_t dx = Sub(rv::As<records::SpriteHead>(src)->x, rv::As<records::BattleTask>(s)->x);
        const int heading = MH_CALL(Math_Ratan2)(AsFloat(dx), AsFloat(dz));
        rv::Current<records::BattleTask>()->step_x = heading;
    }
    const int near = MH_CALL(MagicFx_NearSprite)(src, 0x20000);
    s = Sc();
    if (near != 0) {
        if (s[0xB] == 0) {
            MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
            s = Sc();
        }
        Inc(s[2]);
        return;
    }
    rv::Store(rv::As<records::BattleTask>(s)->step_z, Sub(rv::As<records::BattleTask>(s)->step_z & 0xFFF, rv::As<records::BattleTask>(s)->step_x & 0xFFF));
    s = Sc();
    if (rv::As<records::BattleTask>(s)->step_z < 0) {
        rv::Store(rv::As<records::BattleTask>(s)->step_z, Sub(0, rv::As<records::BattleTask>(s)->step_z));
        s = Sc();
    }
    const std::int32_t turned = rv::As<records::BattleTask>(s)->step_z;
    if (turned <= 0x600 || turned >= 0xA00) return;
    if (s[0xB] == 0) {
        MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
        s = Sc();
    }
    Inc(s[2]);
}

// original 0x49C830: every blade but the lead: the owner's +0xB down, freed.
// The lead: eight sparks (kind 1, 0x53, +1 1, +0xB i, +9 Rand & 15 + 1) and a
// flash (+1 2), each owned by this task's owner and counted in its +0xB;
// sound 0x102; +9 0x10, +2 on.
S03_EXPORT void __cdecl MindSwordBlade_Burst(void) {
    if (Sc()[0xB] != 0) {
        Dec(Owner()[0xB]);
        MH_CALL(BattleTask_FreeCurrent)();
        return;
    }
    for (unsigned i = 0; i < 8; ++i) {
        const unsigned slot = NewTask(0x53);
        unsigned char* const owner = Owner();
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        tchild->owner = static_cast<std::int32_t>(Key(owner));
        child[1] = 1;
        child[0xB] = static_cast<unsigned char>(i);
        const std::uint32_t r = RandCall();
        child[9] = static_cast<unsigned char>((r & 0xF) + 1);
        Inc(Owner()[0xB]);
    }
    {
        const unsigned slot = NewTask(0x53);
        unsigned char* const owner = Owner();
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        tchild->owner = static_cast<std::int32_t>(Key(owner));
        child[1] = 2;
        Inc(owner[0xB]);
    }
    MH_CALL(Sound_PlayById)(0x102);
    Sc()[9] = 0x10;
    Inc(Sc()[2]);
}

// original 0x49C910: the lead blade on layer 3. Three semi-transparent
// gouraud quads (tpage 0x55) fanned round the screen point +0x2E / +0x30 at
// the angle +0x14 + 0x400 i + 0x200.., their far points at the radii of
// MindSword_BladeRadii[1..3] (read in place), their near ones at 8; then two
// quads (tpage 0x35) of radius 0x40 / 0x38, the blade itself.
S03_EXPORT void __cdecl MindSwordBlade_DrawLead(void) {
    DrawModeCommit(0x55, 3);
    {
        const unsigned char* const s = Sc();
        SetSW(4, 8);
        SetSW(0, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x));
        SetSW(2, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_y));
    }
    static constexpr Shades kFan = {{{0x10, 0x40, 0x10}, {0x10, 0x40, 0x10}, {0x10, 0x40, 0x10}, {0x40, 0x40, 0x40}}};
    for (unsigned i = 2; i < 8; i += 2) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetSW(6, Word(Mem(::bof3::Img(MindSword_BladeRadii_addr) + (i / 2) * 2)));
        AngleVertex(p, 8, BladeAngle((i + 1) << 9), 6);
        AngleVertex(p, 0x18, BladeAngle(i << 9), 4);
        AngleVertex(p, 0x28, BladeAngle((i + 2) << 9), 4);
        PutFloat(p + 0x38, SS(0));
        PutFloat(p + 0x3C, SS(2));
        PutShades(p, kFan);
        MH_CALL(Gfx_CommitPrim)(3, 0x44);
    }
    DrawModeCommit(0x35, 3);
    static constexpr Shades kFirst = {{{0x20, 0x20, 0xE0}, {0x20, 0x20, 0xE0}, {0xE0, 0xE0, 0xE0}, {0x20, 0x20, 0xE0}}};
    static constexpr Shades kSecond = {{{0x20, 0x20, 0xE0}, {0x20, 0x20, 0xE0}, {0x80, 0x80, 0x80}, {0x20, 0x20, 0xE0}}};
    BladePair(kFirst, kSecond);
}

// original 0x49CF10: the other blades on layer 3: the lead's two quads (tpage
// 0x35) in their own shades.
S03_EXPORT void __cdecl MindSwordBlade_Draw(void) {
    DrawModeCommit(0x35, 3);
    {
        const unsigned char* const s = Sc();
        SetSW(4, 8);
        SetSW(0, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x));
        SetSW(2, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_y));
    }
    static constexpr Shades kShades = {{{0x10, 0x10, 0x60}, {0x10, 0x10, 0x60}, {0x10, 0x10, 0x30}, {0x10, 0x10, 0x30}}};
    BladePair(kShades, kShades);
}

// original 0x49D300: a three-entry stack table by +2 (_Start, _Grow,
// MAGIC039's 0x4A5180); then while +0 and +2 are set the screen point and the
// spark.
S03_EXPORT void __cdecl MindSwordSpark_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::MindSwordSpark_Start, bof3::addr::MindSwordSpark_Grow,
                                                bof3::addr::FocusMote_Rise};
    const unsigned phase = Sc()[2];
    if (phase >= 3) PastTable("MindSwordSpark_Run", phase, 3);
    magic_harness::Phase(kSteps[phase])();
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(BattleActor_UpdateScreenXY)();
    Call0(bof3::addr::MindSwordSpark_Draw);
}

// original 0x49D350: +9 down; at 0 the spark at 12 x (cos, sin) of the angle
// +0xB << 9 from the source sprite, at its height; +9 0, +0xA 4, +2 on.
S03_EXPORT void __cdecl MindSwordSpark_Start(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    const unsigned angle = (static_cast<unsigned>(Sc()[0xB]) << 9) & 0xFFFF;
    SetSW(8, angle);
    unsigned char* const src = Src();
    int v = MH_CALL(Math_Cos)(static_cast<short>(angle));
    rv::Store(rv::Current<records::BattleTask>()->x, Add(ShlSar(static_cast<std::uint32_t>(v) * 3u, 14, 12), rv::As<records::SpriteHead>(src)->x));
    v = MH_CALL(Math_Sin)(SS(8));
    rv::Store(rv::Current<records::BattleTask>()->z, Add(ShlSar(static_cast<std::uint32_t>(v) * 3u, 14, 12), rv::As<records::SpriteHead>(src)->z));
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(src)->height);
    Sc()[9] = 0;
    Sc()[0xA] = 4;
    Inc(Sc()[2]);
}

// original 0x49D400: +0xA up on odd frames; +9 up by 0x10; at 0x80 +2 on.
S03_EXPORT void __cdecl MindSwordSpark_Grow(void) {
    if (Frame_Counter & 1) Inc(Sc()[0xA]);
    unsigned char* const s = Sc();
    s[9] = static_cast<unsigned char>(s[9] + 0x10);
    if (s[9] == 0x80) Inc(s[2]);
}

// original 0x49D430: one semi-transparent gouraud quad (tpage 0x35) round the
// screen point, +0xA wide either side and +9 tall above it, pale at the top
// and orange below; linked at the task's point, layer offset 0.
S03_EXPORT void __cdecl MindSwordSpark_Draw(void) {
    {
        const unsigned char* const s = Sc();
        SetSW(4, s[9]);
        SetSW(6, s[0xA]);
        SetSW(0, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x));
        unsigned char* const prim = Gfx_PacketNext;
        SetSW(2, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_y));
        MH_CALL(Gpu_SetDrawMode)(prim, 0, 1, 0x35, 0);
    }
    LinkAtSprite(0, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyG4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    PutFloat(p + 8, SS(0) - SS(6));
    PutFloat(p + 0xC, SS(2) - SS(4));
    PutFloat(p + 0x18, SS(0) + SS(6));
    PutFloat(p + 0x1C, SS(2) - SS(4));
    PutFloat(p + 0x28, SS(0) - SS(6));
    PutFloat(p + 0x2C, SS(2));
    PutFloat(p + 0x38, SS(0) + SS(6));
    PutFloat(p + 0x3C, SS(2));
    static constexpr Shades kShades = {{{1, 1, 1}, {1, 1, 1}, {0x80, 0x20, 1}, {0x80, 0x20, 1}}};
    PutShades(p, kShades);
    LinkAtSprite(0, 0x44);
}

// original 0x49D5C0: a four-entry stack table by +2 (_Start, _Grow,
// BarrierLine_Wait, MAGIC060's 0x4B1740); then while +0 and +2 are set the
// flash under the task's matrix.
S03_EXPORT void __cdecl MindSwordFlash_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::MindSwordFlash_Start, bof3::addr::MindSwordFlash_Grow,
                                                bof3::addr::BarrierLine_Wait, bof3::addr::MagicFx_CountDownRelease};
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("MindSwordFlash_Run", phase, 4);
    magic_harness::Phase(kSteps[phase])();
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::MindSwordFlash_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x49D620: the flash at the source sprite; +9 0, +2 on.
S03_EXPORT void __cdecl MindSwordFlash_Start(void) {
    const unsigned char* const src = Src();
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(src)->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(src)->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(src)->height);
    Sc()[9] = 0;
    Inc(Sc()[2]);
}

// original 0x49D660: +9 up by 4; at 0x10 +2 on.
S03_EXPORT void __cdecl MindSwordFlash_Grow(void) {
    unsigned char* const s = Sc();
    s[9] = static_cast<unsigned char>(s[9] + 4);
    if (s[9] == 0x10) Inc(s[2]);
}

// original 0x49D680: a disc of sixteen semi-transparent gouraud triangles
// (tpage 0x35) of radius 0x100 in the task's plane, projected
// (Gte_RotTransPers3); the centre (+9 x 8, +9 x 2, 1) read back from the
// scratch words, the rim 1; layer 5, between two draw-mode packets.
S03_EXPORT void __cdecl MindSwordFlash_Draw(void) {
    DrawModeCommit(0x35, 5);
    {
        const unsigned char* const s = Sc();
        SetSW(0, 0x100);
        SetSW(0xA, static_cast<unsigned>(s[9]) << 3);
        SetSW(0xC, static_cast<unsigned>(s[9]) << 1);
        SetSW(0xE, 1);
    }
    int v = MH_CALL(Math_Sin)(0);
    SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(0))));
    v = MH_CALL(Math_Cos)(0);
    SetVW(0x14, 0);
    SetVW(0x12, static_cast<unsigned>(Mul12(v, SS(0))));
    SetVW(0xC, 0);
    SetVW(4, 0);
    for (int a = 0x100; a < 0x1100; a += 0x100) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const std::uint16_t x = VW(0x10), y = VW(0x12);
        SetVW(0, 0);
        SetVW(2, 0);
        SetVW(8, x);
        SetVW(0xA, y);
        v = MH_CALL(Math_Sin)(a);
        SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(0))));
        v = MH_CALL(Math_Cos)(a);
        SetVW(0x12, static_cast<unsigned>(Mul12(v, SS(0))));
        {
            long depth, flag;
            S03_AS(Rtp3Fn, Gte_RotTransPers3)(VP(0), VP(8), VP(0x10), p + 8, p + 0x18, p + 0x28, &depth, &flag);
        }
        MH_CALL(Gte_PrimDepths3_10B)(p);
        p[4] = SB(0xA);
        p[5] = SB(0xC);
        p[6] = SB(0xE);
        for (unsigned k = 0x14; k < 0x17; ++k) p[k] = 1;
        for (unsigned k = 0x24; k < 0x27; ++k) p[k] = 1;
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
    DrawModeCommit(0x15, 5);
}

void MindSwordBlade_Inject() {
    BOF3_INJECT(MindSword_Task);
    BOF3_INJECT(MindSword_Start);
    BOF3_INJECT(MindSwordChild_Task);
    BOF3_INJECT(MindSwordBlade_Run);
    BOF3_INJECT(MindSwordBlade_Appear);
    BOF3_INJECT(MindSwordBlade_Spin);
    BOF3_INJECT(MindSwordBlade_Wait);
    BOF3_INJECT(MindSwordBlade_Fly);
    BOF3_INJECT(MindSwordBlade_Burst);
    BOF3_INJECT(MindSwordBlade_DrawLead);
    BOF3_INJECT(MindSwordBlade_Draw);
    BOF3_INJECT(MindSwordSpark_Run);
    BOF3_INJECT(MindSwordSpark_Start);
    BOF3_INJECT(MindSwordSpark_Grow);
    BOF3_INJECT(MindSwordSpark_Draw);
    BOF3_INJECT(MindSwordFlash_Run);
    BOF3_INJECT(MindSwordFlash_Start);
    BOF3_INJECT(MindSwordFlash_Grow);
    BOF3_INJECT(MindSwordFlash_Draw);
}

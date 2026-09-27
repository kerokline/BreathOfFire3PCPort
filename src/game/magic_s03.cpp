// Three spell overlays compiled into the exe, round nine group S03
// (docs/magic_s03.md): the PSX's MAGIC006, MAGIC009 and MAGIC012.EMI,
// Magic_Rows rows 109, 51 and 71. Read one id down (docs/cut-content.md
// section 2) the sibling labels them Mind Sword, Chlorine and Blitz; the names
// below use those labels as hypotheses, and say what the code does.
//
//   - MAGIC006 0x49C3D0..0x49D831: four blades (kind 1, 0x53) rise over the
//     caster, spin, and fly at the source sprite; the first to arrive flags the
//     target 0x10 and bursts into eight sparks and a flash (the same kind,
//     phases 1 and 2);
//   - MAGIC009 0x49D840..0x49DEEE: the acting actor's record copied into a
//     child that plays its animation, CLUT row 26's second sixteen words set
//     semi-transparent, then three clouds (kind 1, 0x2A) that open round the
//     source sprite;
//   - MAGIC012 0x49E000..0x49E9E2: one bolt (kind 1, 0x37) per live actor of
//     the targeted side, each flying from the caster to its actor three times;
//     at the end the acting actor's word +0x98 (party) / +0xA4 (enemy) halved,
//     at least 1.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table, and ChlorineCloud_Start's offset index past its three-entry
// table, abort where the original would read on (docs/magic_fx_reached.md
// section 3, the precedent).
#include "game/magic_s03.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// The scratch the overlays keep their working values in: DamageScratch's
// sixteen bytes (0x903850..0x90385F, words or dwords by function) and the
// SVECTORs of Prim_VertexScratch (0x9037A0..). Both are read again after every
// call, as the originals read them.
constexpr std::uint32_t kS = 0x903850;
constexpr std::uint32_t kV = 0x9037A0;

// The overlays' own .data the functions read in place (symbols.toml [[data]]).
constexpr std::uint32_t kBladeRadii = 0x65A5EC;   // MindSword_BladeRadii: four words, [1..3] read
constexpr std::uint32_t kCloudOffsets = 0x65A608; // ChlorineCloud_Offsets: three (dx, dz, dy) dwords
constexpr unsigned kCloudOffsetCount = 3;

// CLUT row 26 of Gfx_ClutStrip (and of its source), in words.
constexpr unsigned kRow26 = 0x1A00;

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(at::kOwner); }
unsigned char* Src() { return Pointer(at::kSource); }
unsigned char TargetByte() { return Mem(at::kTarget)[0]; }
unsigned ActorByte() { return static_cast<unsigned>(Long(Mem(at::kActor))) & 0xFF; }

std::uint16_t SW(unsigned k) { return Word(Mem(kS + k)); }
short SS(unsigned k) { return static_cast<short>(SW(k)); }
unsigned char SB(unsigned k) { return Mem(kS + k)[0]; }
void SetSW(unsigned k, unsigned v) { SetWord(Mem(kS + k), v & 0xFFFF); }
std::int32_t SD(unsigned k) { return Long(Mem(kS + k)); }
void SetSD(unsigned k, std::uint32_t v) { SetLong(Mem(kS + k), static_cast<std::int32_t>(v)); }

std::uint16_t VW(unsigned k) { return Word(Mem(kV + k)); }
void SetVW(unsigned k, unsigned v) { SetWord(Mem(kV + k), v & 0xFFFF); }
const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(kV + k)); }

short S16(const unsigned char* at) { return static_cast<short>(Word(at)); }
void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
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

unsigned char* TaskSlot(unsigned slot) { return Mem(at::kTasks + slot * at::kTaskStride); }
// The originals index the records unchecked: the party's by the index, the
// enemies' by the battle index - 3 (the acting actor) or by an enemy index
// (Blitz's bolts).
unsigned char* PartyRecord(unsigned i) { return Mem(at::kParty + i * at::kPartyStride); }
unsigned char* EnemyByBattleIndex(unsigned battle_index) {
    return Mem(at::kEnemies + (battle_index - 3u) * at::kEnemyStride);
}
unsigned char* EnemyRecord(unsigned i) { return Mem(at::kEnemies + i * at::kEnemyStride); }
unsigned char* ActorRecord(unsigned a) { return a < 3 ? PartyRecord(a) : EnemyByBattleIndex(a); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }
unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
using Fn2 = void (__cdecl*)(unsigned, unsigned);
void Call0(std::uint32_t address) { MH_AT(Fn0, address)(); }
void Call2(std::uint32_t address, unsigned a, unsigned b) { MH_AT(Fn2, address)(a, b); }

// Capcom's, unnamed, in no group: turns the dx / dz pair +0xC / +0x10 of the
// task it is given by its direction byte +8 (docs/magic_s22.md).
constexpr std::uint32_t kTurnOffset = 0x446770;
using TaskFn = void (__cdecl*)(unsigned char*);
void Turn(unsigned char* task) { MH_AT(TaskFn, kTurnOffset)(task); }

// The phase handlers of other units a table holds (docs/magic_s03.md
// section 3) are ours now and named in the tables: S12's
// MagicFx_CountDownRelease, S07's FocusMote_Rise / EnlightenRays_Fade, S11's
// MagicFx_UncountAndFree.

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
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(s + 0x34)), static_cast<unsigned long>(Long(s + 0x38)), dy,
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

// A blade's direction and speed from its task's +0xC..: the owner's position
// + the pair 0x446770 turned, the height the owner's + 0x100, +0xB Rand & 7,
// +9 0x10, +0xA 0, +2 on (BlitzBolt_Start and _Next, which differ in the
// launch distance).
void LaunchBolt(std::int32_t distance) {
    Sc()[8] = Owner()[8];
    SetLong(Sc() + 0xC, distance);
    SetLong(Sc() + 0x10, 0);
    Turn(Sc());
    SetLong(Sc() + 0x34, Add(Long(Owner() + 0x34), Long(Sc() + 0xC)));
    SetLong(Sc() + 0x38, Add(Long(Owner() + 0x38), Long(Sc() + 0x10)));
    SetWord(Sc() + 0x3E, Word(Owner() + 0x3E) + 0x100u);
    const std::uint32_t r = RandCall();
    Sc()[0xB] = static_cast<unsigned char>(r & 7);
    Sc()[9] = 0x10;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
}

// The record Blitz's bolt flies at: its +4 an enemy index when the target
// byte has 0x40, else a party index.
unsigned char* BoltRecord(const unsigned char* s) {
    return TargetByte() & 0x40 ? EnemyRecord(s[4]) : PartyRecord(s[4]);
}

// CLUT row 26's words `from`..`to` - 1 from the source with the
// semi-transparency bit set.
void Row26Stp(unsigned from, unsigned to) {
    for (unsigned k = kRow26 + from; k < kRow26 + to; ++k)
        Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
}

}  // namespace

#define S03_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC006 (row 109, Mind Sword read one id down)

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
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    Inc(Sc()[1]);
    for (unsigned i = 0; i < 4; ++i) {
        const unsigned slot = NewTask(0x53);
        unsigned char* const s = Sc();
        unsigned char* const child = TaskSlot(slot);
        SetLong(child + 0x80, static_cast<std::int32_t>(Key(s)));
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
    SetLong(s + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, Add(Long(Owner() + 0x3C), 0x800000));
    MH_CALL(BattleActor_UpdateScreenXY)();
    unsigned char* const src = Src();
    s = Sc();
    {
        const std::int32_t dz = Sub(Long(src + 0x38), Long(s + 0x38));
        const std::int32_t dx = Sub(Long(src + 0x34), Long(s + 0x34));
        const int heading = MH_CALL(Math_Ratan2)(AsFloat(dx), AsFloat(dz));
        SetLong(Sc() + 0xC, heading);
    }
    SetLong(Sc() + 0x10, 0);
    s = Sc();
    {
        const int dy = S16(src + 0x30) - S16(s + 0x30);
        const int dx = S16(src + 0x2E) - S16(s + 0x2E);
        const int angle = MH_CALL(Math_Ratan2)(AsFloat(dx), AsFloat(dy));
        SetLong(Sc() + 0x14, angle);
    }
    s = Sc();
    SetLong(s + 0x14, Sub(Long(s + 0x14), 0x200) & 0xFFF);
    MH_CALL(Sound_PlayById)(0x100);
    Sc()[9] = 0x20;
    Inc(Sc()[2]);
}

// original 0x49C680: the screen angle +0x14 on by 0x80; +9 down; at 0 +9 =
// +0xB + 12 and +2 on.
S03_EXPORT void __cdecl MindSwordBlade_Spin(void) {
    unsigned char* const s = Sc();
    SetLong(s + 0x14, Add(Long(s + 0x14), 0x80));
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
        const unsigned x = static_cast<unsigned>((Long(src + 0x34) >> 9) - 0x4000);
        const unsigned z = static_cast<unsigned>((Long(src + 0x38) >> 9) - 0x4000);
        const unsigned y = static_cast<unsigned>(Add(Long(src + 0x3C), 0x800000) >> 17);
        // the fourth word is the original's uninitialised stack; the callee reads none of it
        MH_CALL(MagicFx_StepTowardPoint)(x, z, y, 0, 0x60);
    }
    unsigned char* s = Sc();
    SetLong(s + 0x10, Long(s + 0xC));
    s = Sc();
    {
        const std::int32_t dz = Sub(Long(src + 0x38), Long(s + 0x38));
        const std::int32_t dx = Sub(Long(src + 0x34), Long(s + 0x34));
        const int heading = MH_CALL(Math_Ratan2)(AsFloat(dx), AsFloat(dz));
        SetLong(Sc() + 0xC, heading);
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
    SetLong(s + 0x10, Sub(Long(s + 0x10) & 0xFFF, Long(s + 0xC) & 0xFFF));
    s = Sc();
    if (Long(s + 0x10) < 0) {
        SetLong(s + 0x10, Sub(0, Long(s + 0x10)));
        s = Sc();
    }
    const std::int32_t turned = Long(s + 0x10);
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
        SetLong(child + 0x80, static_cast<std::int32_t>(Key(owner)));
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
        SetLong(child + 0x80, static_cast<std::int32_t>(Key(owner)));
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
        SetSW(0, Word(s + 0x2E));
        SetSW(2, Word(s + 0x30));
    }
    static constexpr Shades kFan = {{{0x10, 0x40, 0x10}, {0x10, 0x40, 0x10}, {0x10, 0x40, 0x10}, {0x40, 0x40, 0x40}}};
    for (unsigned i = 2; i < 8; i += 2) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetSW(6, Word(Mem(kBladeRadii + (i / 2) * 2)));
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
        SetSW(0, Word(s + 0x2E));
        SetSW(2, Word(s + 0x30));
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
    SetLong(Sc() + 0x34, Add(ShlSar(static_cast<std::uint32_t>(v) * 3u, 14, 12), Long(src + 0x34)));
    v = MH_CALL(Math_Sin)(SS(8));
    SetLong(Sc() + 0x38, Add(ShlSar(static_cast<std::uint32_t>(v) * 3u, 14, 12), Long(src + 0x38)));
    SetLong(Sc() + 0x3C, Long(src + 0x3C));
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
        SetSW(0, Word(s + 0x2E));
        unsigned char* const prim = Gfx_PacketNext;
        SetSW(2, Word(s + 0x30));
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
    SetLong(Sc() + 0x34, Long(src + 0x34));
    SetLong(Sc() + 0x38, Long(src + 0x38));
    SetLong(Sc() + 0x3C, Long(src + 0x3C));
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

// ===========================================================================
// MAGIC009 (row 51, Chlorine read one id down)

// original 0x49D840: the kind-2 task. A five-entry stack table by +1:
// Chlorine_Start, _Release, SpellFx_Countdown, _WaitChildren, _End.
S03_EXPORT void __cdecl Chlorine_Task(void) {
    static constexpr std::uint32_t kPhases[5] = {bof3::addr::Chlorine_Start, bof3::addr::Chlorine_Release,
                                                 bof3::addr::SpellFx_Countdown, bof3::addr::Chlorine_WaitChildren,
                                                 bof3::addr::Chlorine_End};
    const unsigned phase = Sc()[1];
    if (phase >= 5) PastTable("Chlorine_Task", phase, 5);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x49D880: +0xB 0, +1 on; the acting actor's record +0x4B 0xFF;
// BattleActor_SetAnimation(0x2C, 6); a copy (kind 1, 0x2A) of the acting
// actor's record's first 0x80 bytes (read again after the create), with +0x80
// this task, +1 1, +2 0, +6 1, +5 0x2A, counted in +0xB; CLUT row 26's words
// 0x21..0x2F with their STP bit, word 0x20 0; the owner's +0 bit 0x40.
S03_EXPORT void __cdecl Chlorine_Start(void) {
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
    ActorRecord(ActorByte())[0x4B] = 0xFF;
    MH_CALL(BattleActor_SetAnimation)(0x2C, 6);
    const unsigned slot = NewTask(0x2A);
    const unsigned char* const from = ActorRecord(ActorByte());
    unsigned char* const child = TaskSlot(slot);
    // rep movsd: 32 dwords, forward
    for (unsigned k = 0; k < 0x80; k += 4) std::memmove(child + k, from + k, 4);
    unsigned char* const s = Sc();
    SetLong(child + 0x80, static_cast<std::int32_t>(Key(s)));
    child[1] = 1;
    child[2] = 0;
    child[6] = 1;
    child[5] = 0x2A;
    Inc(s[0xB]);
    Row26Stp(0x21, 0x30);
    Gfx_ClutStrip[kRow26 + 0x20] = 0;
    Gfx_ClutStripDirty = 1;
    Owner()[0] |= 0x40;
}

// original 0x49D9B0: once the copy is gone (+0xB 0): three clouds (kind 1,
// 0x2A, +1 0, +0xB i, +9 4i + 1) counted in +0xB; BattleActor_SetAnimation(4,
// 0); the owner's +0 bit 0x40 cleared; sound 0x100; +9 8, +1 on.
S03_EXPORT void __cdecl Chlorine_Release(void) {
    if (Sc()[0xB] != 0) return;
    for (unsigned i = 0; i < 3; ++i) {
        const unsigned slot = NewTask(0x2A);
        unsigned char* const s = Sc();
        unsigned char* const child = TaskSlot(slot);
        SetLong(child + 0x80, static_cast<std::int32_t>(Key(s)));
        child[1] = 0;
        child[0xB] = static_cast<unsigned char>(i);
        child[9] = static_cast<unsigned char>((i << 2) + 1);
        Inc(s[0xB]);
    }
    MH_CALL(BattleActor_SetAnimation)(4, 0);
    Owner()[0] &= 0xBF;
    MH_CALL(Sound_PlayById)(0x100);
    Sc()[9] = 8;
    Inc(Sc()[1]);
}

// original 0x49DA50: +1 on once +0xB (the children) is 0.
S03_EXPORT void __cdecl Chlorine_WaitChildren(void) {
    unsigned char* const s = Sc();
    if (s[0xB] == 0) Inc(s[1]);
}

// original 0x49DA60: the flags word 0x904AA8 |= 0x2004; the target flagged
// 0x40; the task freed.
S03_EXPORT void __cdecl Chlorine_End(void) {
    const unsigned char target = TargetByte();
    SetWord(Mem(at::kFlags), Word(Mem(at::kFlags)) | 0x2004u);
    MH_CALL(Battle_SetTargetFlag40)(target);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x49DA80: the kind-1 task, a jmp through ChlorineChild_Kinds (two
// entries) by +1, unchecked.
S03_EXPORT void __cdecl ChlorineChild_Task(void) {
    static constexpr std::uint32_t kKinds[2] = {bof3::addr::ChlorineCloud_Run, bof3::addr::ChlorineCopy_Task};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("ChlorineChild_Task", phase, 2);
    magic_harness::Phase(kKinds[phase])();
}

// original 0x49DAA0: a call through ChlorineCloud_Steps (three entries) by
// +2, unchecked; then while +0 and +2 are set the cloud (a tail jmp).
S03_EXPORT void __cdecl ChlorineCloud_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::ChlorineCloud_Start, bof3::addr::ChlorineCloud_Grow,
                                                bof3::addr::EnlightenRays_Fade};
    const unsigned phase = Sc()[2];
    if (phase >= 3) PastTable("ChlorineCloud_Run", phase, 3);
    magic_harness::Phase(kSteps[phase])();
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    Call0(bof3::addr::ChlorineCloud_Draw);
}

// original 0x49DAD0: +9 down; at 0 the source sprite's direction; the offset
// ChlorineCloud_Offsets[+0xB] (read in place) turned by it from the source
// sprite, its dy on the source's height; the screen point; +0xA 0, +2 on.
S03_EXPORT void __cdecl ChlorineCloud_Start(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    const unsigned char* const src = Src();
    Sc()[8] = src[8];
    auto offset = [](unsigned field) {
        const unsigned i = Sc()[0xB];
        if (i >= kCloudOffsetCount) bof3::Fatal("ChlorineCloud_Start: offset %u, past the %u-entry table", i, kCloudOffsetCount);
        return Long(Mem(kCloudOffsets + 12 * i + field));
    };
    SetLong(Sc() + 0xC, offset(0));
    SetLong(Sc() + 0x10, offset(4));
    Turn(Sc());
    SetLong(Sc() + 0x34, Add(Long(Sc() + 0xC), Long(src + 0x34)));
    SetLong(Sc() + 0x38, Add(Long(Sc() + 0x10), Long(src + 0x38)));
    {
        const std::int32_t dy = offset(8);
        SetLong(Sc() + 0x3C, Add(dy, Long(src + 0x3C)));
    }
    MH_CALL(BattleActor_UpdateScreenXY)();
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
}

// original 0x49DB90: +9 and +0xA up by 2; at a +9 of 0x10 +2 on. (Reached by
// nine overlays' tables.)
S03_EXPORT void __cdecl ChlorineCloud_Grow(void) {
    unsigned char* const s = Sc();
    s[9] = static_cast<unsigned char>(s[9] + 2);
    s[0xA] = static_cast<unsigned char>(s[0xA] + 2);
    if (s[9] == 0x10) Inc(s[2]);
}

// original 0x49DBC0: one semi-transparent textured quad (tpage 0x340 / 0x100,
// CLUT row 0x1FA, x 0x20) round the screen point, its corners at the angles
// 0x200, 0x600, 0xE00, 0xA00 and the radius +9 x 2 (the dword 0x903850, read
// back after every call); shade (+0xA x 10, 0, +0xA x 9); linked at the
// task's point, layer offset 2.
S03_EXPORT void __cdecl ChlorineCloud_Draw(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    LinkAtSprite(2, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    SetSD(0, static_cast<std::uint32_t>(Sc()[9]) << 1);
    MH_CALL(Gpu_SetPolyFT4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    static constexpr int kAngles[4] = {0x200, 0x600, 0xE00, 0xA00};
    for (unsigned k = 0; k < 4; ++k) {
        int v = MH_CALL(Math_Cos)(kAngles[k]);
        PutFloat(p + 8 + 0x10 * k, Add(Mul12(v, SD(0)), S16(Sc() + 0x2E)));
        v = MH_CALL(Math_Sin)(kAngles[k]);
        PutFloat(p + 0xC + 0x10 * k, Add(Mul12(v, SD(0)), S16(Sc() + 0x30)));
    }
    const unsigned tpage = MH_CALL(Gpu_GetTPage)(0, 1, 0x340, 0x100);
    SetWord(p + 0x26, tpage);
    const unsigned clut = MH_CALL(Gpu_GetClut)(0x20, 0x1FA);
    SetWord(p + 0x16, clut);
    p[0x14] = 0x80;
    p[0x34] = 0x80;
    p[0x15] = 0x20;
    p[0x24] = 0xA0;
    p[0x25] = 0x20;
    p[0x35] = 0x40;
    p[0x44] = 0xA0;
    p[0x45] = 0x40;
    p[5] = 0;
    p[4] = static_cast<unsigned char>(Sc()[0xA] * 10u);
    p[6] = static_cast<unsigned char>(Sc()[0xA] * 9u);
    LinkAtSprite(2, 0x48);
}

// original 0x49DE00: the copy's kind-1 task. A four-entry stack table by +2
// (_Size, _Play, _Wait, BattleFx_FreeTask); then with +0 set the sprite's
// screen update.
S03_EXPORT void __cdecl ChlorineCopy_Task(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::ChlorineCopy_Size, bof3::addr::ChlorineCopy_Play,
                                                bof3::addr::ChlorineCopy_Wait, bof3::addr::BattleFx_FreeTask};
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("ChlorineCopy_Task", phase, 4);
    magic_harness::Phase(kSteps[phase])();
    if (Sc()[0] != 0) MH_CALL(Sprite_UpdateScreen)();
}

// original 0x49DE50: +9 = BattleActor_FxSizeB(); +2 on.
S03_EXPORT void __cdecl ChlorineCopy_Size(void) {
    const unsigned char size = MH_CALL(BattleActor_FxSizeB)();
    Sc()[9] = size;
    Inc(Sc()[2]);
}

// original 0x49DE70: +9 (unless 0xFF) down, at 0 BattleActor_PlaySound(2, 5);
// the script ticked once; at its end +9 8 and +2 on.
S03_EXPORT void __cdecl ChlorineCopy_Play(void) {
    {
        unsigned char* const s = Sc();
        const unsigned char b = s[9];
        if (b != 0xFF) {
            s[9] = static_cast<unsigned char>(b - 1);
            if (Sc()[9] == 0) MH_CALL(BattleActor_PlaySound)(2, 5);
        }
    }
    if (MH_CALL(Sprite_ScriptTickOnce)() == 0) return;
    Sc()[9] = 8;
    Inc(Sc()[2]);
}

// original 0x49DEC0: +9 down; at 0 the owner's +0xB down and +2 on.
S03_EXPORT void __cdecl ChlorineCopy_Wait(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Dec(Owner()[0xB]);
    Inc(Sc()[2]);
}

// ===========================================================================
// MAGIC012 (row 71, Blitz read one id down)

// original 0x49E000: the kind-2 task. A two-entry stack table by +1:
// Blitz_Start, Blitz_End.
S03_EXPORT void __cdecl Blitz_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Blitz_Start, bof3::addr::Blitz_End};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("Blitz_Task", phase, 2);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x49E030: the owner's direction and position; +0xB 0; one bolt
// (kind 1, 0x37) per actor of the targeted side not out - the eight enemies
// (battle index i + 3) when the target byte has 0x40, else the three party
// members: +0x80 this task, +3 the battle index, +4 the side's index, +9 1 +
// 8 per bolt made, counted in +0xB. CLUT row 26 with its STP bits, but its
// first word; +1 on.
S03_EXPORT void __cdecl Blitz_Start(void) {
    Sc()[8] = Owner()[8];
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
    Sc()[0xB] = 0;
    const bool enemies = (TargetByte() & 0x40) != 0;
    unsigned char shade_step = 1;
    const unsigned count = enemies ? 8 : 3;
    for (unsigned i = 0; i < count; ++i) {
        const unsigned index = enemies ? i + 3 : i;
        if (MH_CALL(Battle_ActorIsOut)(index) != 0) continue;
        const unsigned slot = NewTask(0x37);
        unsigned char* const s = Sc();
        unsigned char* const child = TaskSlot(slot);
        SetLong(child + 0x80, static_cast<std::int32_t>(Key(s)));
        child[3] = static_cast<unsigned char>(index);
        child[4] = static_cast<unsigned char>(i);
        child[9] = shade_step;
        shade_step = static_cast<unsigned char>(shade_step + 8);
        Inc(s[0xB]);
    }
    Row26Stp(0, 0x100);
    const unsigned short first = Gfx_ClutStripSource[kRow26];
    Gfx_ClutStripDirty = 1;
    Gfx_ClutStrip[kRow26] = first;
    Inc(Sc()[1]);
}

// original 0x49E1C0: once every bolt has ended (+0xB 0): the acting actor's
// word +0x98 (party) / +0xA4 (enemy, battle index - 3) halved, 1 if that made
// it 0; the effect's done flag; the task freed.
S03_EXPORT void __cdecl Blitz_End(void) {
    if (Sc()[0xB] != 0) return;
    const unsigned a = ActorByte();
    unsigned char* const word = a < 3 ? PartyRecord(a) + 0x98 : EnemyByBattleIndex(a) + 0xA4;
    SetWord(word, Word(word) >> 1);
    if (Word(word) == 0) SetWord(word, 1);
    Mem(at::kFlags)[0] |= 4;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x49E240: the bolt's kind-1 task, a jmp through
// BlitzBolt_TaskTable (one entry) by +1, unchecked.
S03_EXPORT void __cdecl BlitzBolt_Task(void) {
    const unsigned phase = Sc()[1];
    if (phase >= 1) PastTable("BlitzBolt_Task", phase, 1);
    magic_harness::Phase(bof3::addr::BlitzBolt_Run)();
}

// original 0x49E260: a jmp through BlitzBolt_Steps (twelve entries) by +2,
// unchecked: _Start, then (_Seek, _Bounce, _Next) three times, MAGIC058's
// 0x4AF490, _Drift.
S03_EXPORT void __cdecl BlitzBolt_Run(void) {
    static constexpr std::uint32_t kSteps[12] = {
        bof3::addr::BlitzBolt_Start, bof3::addr::BlitzBolt_Seek,         bof3::addr::BlitzBolt_Bounce,
        bof3::addr::BlitzBolt_Next,  bof3::addr::BlitzBolt_Seek,         bof3::addr::BlitzBolt_Bounce,
        bof3::addr::BlitzBolt_Next,  bof3::addr::BlitzBolt_Seek,         bof3::addr::BlitzBolt_Bounce,
        bof3::addr::BlitzBolt_Next,  bof3::addr::MagicFx_UncountAndFree, bof3::addr::BlitzBolt_Drift};
    const unsigned phase = Sc()[2];
    if (phase >= 12) PastTable("BlitzBolt_Run", phase, 12);
    magic_harness::Phase(kSteps[phase])();
}

// original 0x49E280: +9 down; at 0 the bolt 0x20000 in front of the owner
// (LaunchBolt), +2 on.
S03_EXPORT void __cdecl BlitzBolt_Start(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    LaunchBolt(0x20000);
}

// original 0x49E340: +0xA up on odd frames; a step (speed 0x40) toward its
// actor's point (the record by +4, the enemies' when the target byte has
// 0x40) at (height + 0xC0) / 2. If that actor is out (Battle_ActorIsOut of
// +3): its point into +0xC..+0x14 and +2 to 11 (_Drift). At the actor
// (MagicFx_NearSprite, 0x8000): byte 0x904AA9 bit 0x20, +3 flagged 0x40,
// sound 0x203, the heading back to the owner in +0xC, +0x14 0x40, +0x20 -8,
// +2 on. Then the screen point and the bolt (BlitzBolt_Draw(+0xB, 0)).
S03_EXPORT void __cdecl BlitzBolt_Seek(void) {
    if (Frame_Counter & 1) Inc(Sc()[0xA]);
    unsigned char* s = Sc();
    unsigned char* const record = BoltRecord(s);
    const int y = (S16(record + 0x3E) + 0xC0) >> 1;
    const int x = (Long(record + 0x34) >> 9) - 0x4000;
    const int z = (Long(record + 0x38) >> 9) - 0x4000;
    if (MH_CALL(Battle_ActorIsOut)(s[3]) != 0) {
        SetLong(Sc() + 0xC, (Long(record + 0x34) >> 9) - 0x4000);
        SetLong(Sc() + 0x10, (Long(record + 0x38) >> 9) - 0x4000);
        SetLong(Sc() + 0x14, (S16(record + 0x3E) + 0xC0) >> 1);
        Sc()[2] = 0xB;
    }
    // the fourth word is the original's uninitialised stack; the callee reads none of it
    MH_CALL(MagicFx_StepTowardPoint)(static_cast<unsigned>(x), static_cast<unsigned>(z), static_cast<unsigned>(y), 0, 0x40);
    if (MH_CALL(MagicFx_NearSprite)(record, 0x8000) != 0) {
        Mem(0x904AA9)[0] |= 0x20;
        MH_CALL(Battle_SetTargetFlag40)(Sc()[3]);
        MH_CALL(Sound_PlayById)(0x203);
        const unsigned char* const owner = Owner();
        s = Sc();
        const std::int32_t dz = Sub(Long(owner + 0x38), Long(s + 0x38));
        const std::int32_t dx = Sub(Long(owner + 0x34), Long(s + 0x34));
        const int heading = MH_CALL(Math_Ratan2)(AsFloat(dx), AsFloat(dz));
        SetLong(Sc() + 0xC, heading);
        SetLong(Sc() + 0x14, 0x40);
        SetLong(Sc() + 0x20, -8);
        Inc(Sc()[2]);
    }
    MH_CALL(BattleActor_UpdateScreenXY)();
    Call2(bof3::addr::BlitzBolt_Draw, Sc()[0xB], 0);
}

// original 0x49E4F0: +0xA up; the position on by 2 x (sin, cos) of the
// heading +0xC; the rise +0x14 on by the fall +0x20 and the height word +0x3E
// by the rise; +9 down, at 0 +2 on. Then the screen point and the bolt
// (BlitzBolt_Draw(+0xB, 1)).
S03_EXPORT void __cdecl BlitzBolt_Bounce(void) {
    Inc(Sc()[0xA]);
    {
        unsigned char* const s = Sc();
        unsigned char* const px = s + 0x34;
        const int v = MH_CALL(Math_Sin)(Long(s + 0xC));
        SetLong(px, Add(Long(px), ShlSar(static_cast<std::uint32_t>(v), 13, 12)));
    }
    {
        unsigned char* const s = Sc();
        unsigned char* const pz = s + 0x38;
        const int v = MH_CALL(Math_Cos)(Long(s + 0xC));
        SetLong(pz, Add(Long(pz), ShlSar(static_cast<std::uint32_t>(v), 13, 12)));
    }
    unsigned char* const s = Sc();
    SetLong(s + 0x14, Add(Long(s + 0x14), Long(s + 0x20)));
    SetWord(s + 0x3E, Word(s + 0x3E) + Word(s + 0x14));
    Dec(s[9]);
    if (s[9] == 0) Inc(s[2]);
    MH_CALL(BattleActor_UpdateScreenXY)();
    Call2(bof3::addr::BlitzBolt_Draw, Sc()[0xB], 1);
}

// original 0x49E590: if +3 is out, +2 to 10 (MAGIC058's free). Else, once its
// actor's state byte +1 is not 6 (the reaction state), the bolt again 0x10000
// in front of the owner (LaunchBolt), +2 on.
S03_EXPORT void __cdecl BlitzBolt_Next(void) {
    if (MH_CALL(Battle_ActorIsOut)(Sc()[3]) != 0) {
        Sc()[2] = 0xA;
        return;
    }
    if (BoltRecord(Sc())[1] == 6) return;
    LaunchBolt(0x10000);
}

// original 0x49E6A0: +0xA up on odd frames; a step (speed 0x30) toward the
// point _Seek left in +0xC..+0x14; +9 down, at 0 +2 back one (to 10,
// MAGIC058's free); the screen point and the bolt (BlitzBolt_Draw(+0xB, 0)).
S03_EXPORT void __cdecl BlitzBolt_Drift(void) {
    if (Frame_Counter & 1) Inc(Sc()[0xA]);
    {
        const unsigned char* const s = Sc();
        // the fourth word is the original's uninitialised stack; the callee reads none of it
        MH_CALL(MagicFx_StepTowardPoint)(static_cast<unsigned>(Long(s + 0xC)), static_cast<unsigned>(Long(s + 0x10)),
                                         static_cast<unsigned>(Long(s + 0x14)), 0, 0x30);
    }
    unsigned char* const s = Sc();
    Dec(s[9]);
    if (s[9] == 0) Dec(s[2]);
    MH_CALL(BattleActor_UpdateScreenXY)();
    Call2(bof3::addr::BlitzBolt_Draw, Sc()[0xB], 0);
}

// original 0x49E720: the bolt, one textured quad (tpage 0x340 / 0x100 with
// the blend `abr`, CLUT row 0x1FA) round the screen point at radius 0x24 and
// the angles (+0xA -3, -5, +3, +5) x 0x100 (the dword 0x903858 read back for
// each sine), its u the eight 32-wide cells by +0xB, its shade +9 x 8 (the
// dword 0x903854); linked at the task's point, layer offset 2. The first
// argument is not read.
S03_EXPORT void __cdecl BlitzBolt_Draw(unsigned unused, unsigned abr) {
    (void)unused;
    const unsigned e = abr & 0xFF;
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, ((e & 3) << 5) | 0x15, 0);
    LinkAtSprite(2, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    SetSD(0, 0x24);
    SetSD(4, static_cast<std::uint32_t>(Sc()[9]) << 3);
    MH_CALL(Gpu_SetPolyFT4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, e);
    static constexpr int kSteps[4] = {-3, -5, 3, 5};
    for (unsigned k = 0; k < 4; ++k) {
        const std::uint32_t angle = ((static_cast<std::uint32_t>(Sc()[0xA]) + static_cast<std::uint32_t>(kSteps[k])) & 0xF)
                                    << 8;
        SetSD(8, angle);
        int v = MH_CALL(Math_Cos)(static_cast<int>(angle));
        PutFloat(p + 8 + 0x10 * k, Add(Mul12(v, SD(0)), S16(Sc() + 0x2E)));
        v = MH_CALL(Math_Sin)(SD(8));
        PutFloat(p + 0xC + 0x10 * k, Add(Mul12(v, SD(0)), S16(Sc() + 0x30)));
    }
    const unsigned tpage = MH_CALL(Gpu_GetTPage)(1, e, 0x340, 0x100);
    SetWord(p + 0x26, tpage);
    const unsigned clut = MH_CALL(Gpu_GetClut)(0, 0x1FA);
    SetWord(p + 0x16, clut);
    p[0x15] = 0;
    p[0x14] = static_cast<unsigned char>(Sc()[0xB] << 5);
    p[0x25] = 0;
    p[0x24] = static_cast<unsigned char>((Sc()[0xB] << 5) + 0x1F);
    p[0x35] = 0x40;
    p[0x34] = static_cast<unsigned char>(Sc()[0xB] << 5);
    p[0x45] = 0x40;
    p[0x44] = static_cast<unsigned char>((Sc()[0xB] << 5) + 0x1F);
    p[4] = SB(4);
    p[5] = SB(4);
    p[6] = SB(4);
    LinkAtSprite(2, 0x48);
}

void MagicS03_Inject() {
    if (bof3::WantsShadow("magic_s03")) magic_s03::SelfTest();
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
    BOF3_INJECT(Chlorine_Task);
    BOF3_INJECT(Chlorine_Start);
    BOF3_INJECT(Chlorine_Release);
    BOF3_INJECT(Chlorine_WaitChildren);
    BOF3_INJECT(Chlorine_End);
    BOF3_INJECT(ChlorineChild_Task);
    BOF3_INJECT(ChlorineCloud_Run);
    BOF3_INJECT(ChlorineCloud_Start);
    BOF3_INJECT(ChlorineCloud_Grow);
    BOF3_INJECT(ChlorineCloud_Draw);
    BOF3_INJECT(ChlorineCopy_Task);
    BOF3_INJECT(ChlorineCopy_Size);
    BOF3_INJECT(ChlorineCopy_Play);
    BOF3_INJECT(ChlorineCopy_Wait);
    BOF3_INJECT(Blitz_Task);
    BOF3_INJECT(Blitz_Start);
    BOF3_INJECT(Blitz_End);
    BOF3_INJECT(BlitzBolt_Task);
    BOF3_INJECT(BlitzBolt_Run);
    BOF3_INJECT(BlitzBolt_Start);
    BOF3_INJECT(BlitzBolt_Seek);
    BOF3_INJECT(BlitzBolt_Bounce);
    BOF3_INJECT(BlitzBolt_Next);
    BOF3_INJECT(BlitzBolt_Drift);
    BOF3_INJECT(BlitzBolt_Draw);
}

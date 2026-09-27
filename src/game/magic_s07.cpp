// Four spell overlays compiled into the exe, round nine group S07
// (docs/magic_s07.md): the PSX's MAGIC021, MAGIC038, MAGIC039 and
// MAGIC040.EMI, Magic_Rows rows 33, 121, 78 and 79. Read one id down
// (docs/cut-content.md section 2) the sibling labels them Bonebreak, War
// Shout, Focus / Meditation and Enlighten; the names below use those labels as
// hypotheses, and say what the code does.
//
//   - MAGIC021 0x4A3B20..0x4A4436: a copy of the acting actor's record as a
//     child (kind 1, 9) whose script runs, then twelve motes from a pool of
//     its own (64 records at 0x679B40, walked by the kind-2 task) that fly out
//     from the source sprite drawing a gouraud fan and ring each, and a screen
//     shade (MAGIC008's 0x4A29C0);
//   - MAGIC038 0x4A4440..0x4A4C06: sixteen motes (kind 1, 0x59) that circle
//     the side's centre, then one buff popup per living actor of the target's
//     side (MagicFx_BuffPopup);
//   - MAGIC039 0x4A4C10..0x4A5A06: an aura (kind 1, 0x3B) that draws a ring of
//     64 projected quads and a disc of 16 projected triangles and tints the
//     source sprite, and eight rising motes; its shades by the ability id (0xA3
//     the second of its two ids);
//   - MAGIC040 0x4A5A10..0x4A6434: rays and a ring (kind 1, 0x3C) at the
//     actor the target byte names, then the buff (MagicFx_ApplyBuff) and its
//     popup task.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table aborts where the original would call through whatever follows
// it (docs/magic_fx_reached.md section 3, the precedent).
#include "game/magic_s07.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

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
// four SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read
// again after every call, as the originals read them.
constexpr std::uint32_t kS = 0x903850;
constexpr std::uint32_t kV = 0x9037A0;

// The cells the overlays read beyond the harness's names.
constexpr std::uint32_t kFrameSet = 0x9039D8;        // the sprite frame-offset table pointer (sprite_pose.h)
constexpr std::uint32_t kFrameSetBattle = 0x8B3580;
constexpr std::uint32_t kFrameSetEffect = 0x8E3580;
constexpr std::uint32_t kAbilityId = 0x904B80;       // u16: the ability id (Battle_StartItemMagic stores it)
constexpr std::uint32_t kFormIndex = 0x904B89;       // u8: the index a party record with +0x134 bit 1 uses

// This group's .data (symbols.toml [[data]]).
constexpr std::uint32_t kMotePool = 0x679B40;           // BonebreakMote_Pool: 64 records of 0x84 bytes
constexpr std::uint32_t kWarShoutAnimations = 0x65A718;  // 16 bytes
constexpr std::uint32_t kDiscShades = 0x65A730;          // FocusAura_DiscShades: 2 x 3 bytes
constexpr std::uint32_t kMoteShades = 0x65A738;          // FocusMote_Shades
constexpr std::uint32_t kRingShades = 0x65A740;          // FocusAura_RingShades
constexpr std::uint32_t kMoteDelays = 0x65A748;          // FocusMote_Delays: 8 bytes
constexpr std::uint32_t kAnchorOffsets = 0x65A760;       // Enlighten_AnchorOffsets: (dx, dy) words
constexpr std::uint32_t kAnchorOffsetsB = 0x65A7B8;      // Enlighten_AnchorOffsetsB
constexpr std::uint32_t kAnchorIndexB = 0x65A810;        // Enlighten_AnchorIndexB: bytes

// The phase handlers and callees of other units (docs/magic_s07.md section 3)
// are ours now and called by name: S05's Magic017_Wait, S06's
// Magic008_DrawFlash, S37's MagicFx_FreeCurrentRecord, S03's
// Chlorine_WaitChildren, S11's MagicFx_UncountAndFree.

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(at::kOwner); }
unsigned char TargetByte() { return Mem(at::kTarget)[0]; }
unsigned char* Source() { return Pointer(at::kSource); }

std::uint16_t SW(unsigned k) { return Word(Mem(kS + k)); }
short SS(unsigned k) { return static_cast<short>(SW(k)); }
unsigned char SB(unsigned k) { return Mem(kS + k)[0]; }
void SetSW(unsigned k, unsigned v) { SetWord(Mem(kS + k), v & 0xFFFF); }
std::int32_t SD(unsigned k) { return Long(Mem(kS + k)); }
void SetSD(unsigned k, std::uint32_t v) { SetLong(Mem(kS + k), static_cast<std::int32_t>(v)); }

std::uint16_t VW(unsigned k) { return Word(Mem(kV + k)); }
short VS(unsigned k) { return static_cast<short>(VW(k)); }
void SetVW(unsigned k, unsigned v) { SetWord(Mem(kV + k), v & 0xFFFF); }
const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(kV + k)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }
std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
void AddLong(unsigned char* at, std::uint32_t v) {
    SetLong(at, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(at)) + v));
}

// `add r32, r32`: the 32-bit sum, wrapped.
std::int32_t Sum(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}
// `imul r32, r32`: the 32-bit product, wrapped.
int Imul(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)); }
// `imul` then `sar 0xC`.
int Mul12(int a, int b) { return Imul(a, b) >> 12; }
// `shl n` on a dword.
int Shl(int v, unsigned n) { return static_cast<int>(static_cast<std::uint32_t>(v) << n); }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(at::kTasks + slot * at::kTaskStride); }
unsigned char* PoolRecord(unsigned index) { return Mem(kMotePool + index * 0x84u); }
// The originals index the records by the battle index, unchecked: the enemy's
// by index - 3.
unsigned char* PartyRecord(unsigned i) { return Mem(at::kParty + i * at::kPartyStride); }
unsigned char* EnemyRecord(unsigned battle_index) {
    return Mem(at::kEnemies + static_cast<std::uint32_t>(static_cast<int>(battle_index) - 3) * at::kEnemyStride);
}

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }
unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions and other units' called by address, as the originals
// call them: in the game the jmp Inject put there (or Capcom's code), in the
// fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
using ByteFn = unsigned char (__cdecl*)();
using Fn2 = void (__cdecl*)(unsigned, unsigned);
void Call0(std::uint32_t address) { MH_AT(Fn0, address)(); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}
// A stack table's (or a .data table's) dispatch: entry `phase`, which the
// originals do not check.
void Dispatch(const std::uint32_t* phases, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    magic_harness::Phase(phases[phase])();
}

// The projections with the arguments the originals push: the depth and a
// flag pointer past the declared ones (cdecl: the caller pops them).
using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, unsigned char*, unsigned char*, unsigned char*,
                               long*, long*);
using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);
#define S07_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

// A primitive linked at the current task's position (+0x34 / +0x38, read at
// the call) on layer `dy`.
void LinkAtSprite(int dy, unsigned size) {
    const unsigned char* const s = Sc();
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(s + 0x34)), static_cast<unsigned long>(Long(s + 0x38)), dy,
                                size);
}
// A draw-mode packet (tpage `tpage`, dithered) at Gfx_PacketNext.
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

// The task's position (+0x34 / +0x38 / +0x3C) the owner's, each read again.
void PositionFromOwner() {
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
}

// Row 26 of Gfx_ClutStrip, its 32 words, back from its source.
void RestoreRow26() {
    for (unsigned k = 0; k < 0x10; ++k) {
        Gfx_ClutStrip[0x1A00 + k] = Gfx_ClutStripSource[0x1A00 + k];
        Gfx_ClutStrip[0x1A10 + k] = Gfx_ClutStripSource[0x1A10 + k];
    }
}

}  // namespace

#define S07_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC021 (row 33, Bonebreak read one id down)

// original 0x4A3B20: the kind-2 task. A three-entry stack table by +1 -
// Bonebreak_Start, MAGIC017's 0x4A13B0, MagicFx_DoneAndFree - then the mote
// pool walked: every record with bit 0 becomes Sprite_Current, its +0x80 the
// owner, for BonebreakMote_Task; both put back after each (as read after the
// phase call).
S07_EXPORT void __cdecl Bonebreak_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::Bonebreak_Start, bof3::addr::Magic017_Wait,
                                                 bof3::addr::MagicFx_DoneAndFree};
    Dispatch(kPhases, 3, Sc()[1], "Bonebreak_Task");
    unsigned char* const self = Sprite_Current;
    const std::int32_t owner = Long(Mem(at::kOwner));
    for (unsigned i = 0; i < 64; ++i) {
        unsigned char* const rec = PoolRecord(i);
        if ((rec[0] & 1) == 0) continue;
        const std::int32_t rec_owner = Long(rec + 0x80);
        Sprite_Current = rec;
        SetLong(Mem(at::kOwner), rec_owner);
        Call0(bof3::addr::BonebreakMote_Task);
        SetLong(Mem(at::kOwner), owner);
        Sprite_Current = self;
    }
}

// original 0x4A3BA0: the pool's records +0..+2 cleared; +0xB 0, +1 on; the
// actor animated (offset 0xC, 2); a child (kind 1, 9) made a copy of the
// acting actor's record's first 0x80 bytes (a party record at 0..2, else the
// enemy's; the actor read after the create), then +0x80 this task, +1 / +2 0,
// +6 1, +5 9; +0xB up; the owner's byte +0 given bit 6.
S07_EXPORT void __cdecl Bonebreak_Start(void) {
    for (unsigned i = 0; i < 64; ++i) {
        unsigned char* const rec = PoolRecord(i);
        rec[0] = 0;
        rec[1] = 0;
        rec[2] = 0;
    }
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
    MH_CALL(BattleActor_SetAnimation)(0xC, 2);
    const unsigned slot = NewTask(9);
    const unsigned actor = Mem(at::kActor)[0];
    const unsigned char* const src = actor <= 2 ? PartyRecord(actor) : EnemyRecord(actor);
    unsigned char* const child = TaskSlot(slot);
    for (unsigned k = 0; k < 0x80; k += 4) SetLong(child + k, Long(src + k));
    unsigned char* const self = Sc();
    SetLong(child + 0x80, static_cast<std::int32_t>(Key(self)));
    child[1] = 0;
    child[2] = 0;
    child[6] = 1;
    child[5] = 9;
    Inc(self[0xB]);
    Owner()[0] |= 0x40;
}

// original 0x4A3C80: the copy's kind-1 task, a jmp through
// BonebreakChild_TaskTable (one entry) by +1, unchecked.
S07_EXPORT void __cdecl BonebreakChild_Task(void) {
    static constexpr std::uint32_t kKinds[1] = {bof3::addr::BonebreakChild_Run};
    Dispatch(kKinds, 1, Sc()[1], "BonebreakChild_Task");
}

// original 0x4A3CA0: a six-entry stack table by +2 - BattleFx_SetSize, _Burst,
// _WaitScript, _Shade, _WaitMotes, BattleFx_FreeTask - then, with byte +0 not
// 0, Sprite_UpdateScreen.
S07_EXPORT void __cdecl BonebreakChild_Run(void) {
    static constexpr std::uint32_t kSteps[6] = {bof3::addr::BattleFx_SetSize,        bof3::addr::BonebreakChild_Burst,
                                                bof3::addr::BonebreakChild_WaitScript, bof3::addr::BonebreakChild_Shade,
                                                bof3::addr::BonebreakChild_WaitMotes,  bof3::addr::BattleFx_FreeTask};
    Dispatch(kSteps, 6, Sc()[2], "BonebreakChild_Run");
    if (Sc()[0] != 0) MH_CALL(Sprite_UpdateScreen)();
}

// original 0x4A3D00: the script ticked once; +9 down; at 0 sound 0x100, +9
// and +0xB 0, and twelve motes from the pool (BonebreakMote_Alloc, the index
// untested): +0x80 this task (read after each alloc), +9 the delay (i / 2) x
// 3 + 1; +0xB up for each; +2 on.
S07_EXPORT void __cdecl BonebreakChild_Burst(void) {
    MH_CALL(Sprite_ScriptTickOnce)();
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Sound_PlayById)(0x100);
    Sc()[9] = 0;
    Sc()[0xB] = 0;
    for (unsigned i = 0; i < 12; ++i) {
        const unsigned index = MH_AT(ByteFn, bof3::addr::BonebreakMote_Alloc)() & 0xFFu;
        unsigned char* const self = Sc();
        unsigned char* const rec = PoolRecord(index);
        SetLong(rec + 0x80, static_cast<std::int32_t>(Key(self)));
        rec[9] = static_cast<unsigned char>((i >> 1) * 3 + 1);
        Inc(self[0xB]);
    }
    Inc(Sc()[2]);
}

// original 0x4A3DA0: the script ticked once; when it reports its end, target
// flag 0x40 and +2 on.
S07_EXPORT void __cdecl BonebreakChild_WaitScript(void) {
    if (MH_CALL(Sprite_ScriptTickOnce)() == 0) return;
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    Inc(Sc()[2]);
}

// original 0x4A3DC0: below +9 0xC MAGIC008's screen shade (0x4A29C0) and +9
// up by 4; else +2 on.
S07_EXPORT void __cdecl BonebreakChild_Shade(void) {
    if (Sc()[9] >= 0xC) {
        Inc(Sc()[2]);
        return;
    }
    Call0(bof3::addr::Magic008_DrawFlash);
    AddB(Sc()[9], 4);
}

// original 0x4A3DE0: once +0xB (the motes alive) is 0, the owner's +0xB down
// and +2 on.
S07_EXPORT void __cdecl BonebreakChild_WaitMotes(void) {
    if (Sc()[0xB] != 0) return;
    Dec(Owner()[0xB]);
    Inc(Sc()[2]);
}

// original 0x4A3E10: a pool record's task, a jmp through
// BonebreakMote_TaskTable (one entry) by +1, unchecked.
S07_EXPORT void __cdecl BonebreakMote_Task(void) {
    static constexpr std::uint32_t kKinds[1] = {bof3::addr::BonebreakMote_Run};
    Dispatch(kKinds, 1, Sc()[1], "BonebreakMote_Task");
}

// original 0x4A3E30: a call through BonebreakMote_Steps (three entries:
// _Launch, MagicFx_CountUp9By2, _Fade) by +2; then with bytes +0 and +2 not 0
// the screen point, the fan, the ring (a tail jump in the original).
S07_EXPORT void __cdecl BonebreakMote_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::BonebreakMote_Launch, bof3::addr::MagicFx_CountUp9By2,
                                                bof3::addr::BonebreakMote_Fade};
    Dispatch(kSteps, 3, Sc()[2], "BonebreakMote_Run");
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(BattleActor_UpdateScreenXY)();
    Call0(bof3::addr::BonebreakMote_DrawFan);
    Call0(bof3::addr::BonebreakMote_DrawRing);
}

// original 0x4A3E70: +9 down; at 0 the mote starts at the source sprite
// (0x904B4C): +0x34 / +0x38 its, word +0x3E its + (Rand & 7) sl 5; the speed
// word 0x903850 (0xC - that) sl 4; the angle word 0x903854 (Rand & 7) sl 9;
// the position moved by sin / cos x speed sar 3 (the speed and the angle read
// again after each call); +9 0, +0xA 0x10, +2 on.
S07_EXPORT void __cdecl BonebreakMote_Launch(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    const unsigned k = RandCall() & 7;
    const unsigned char* const src = Source();
    SetLong(Sc() + 0x34, Long(src + 0x34));
    SetLong(Sc() + 0x38, Long(src + 0x38));
    SetWord(Sc() + 0x3E, ((k << 5) + Word(src + 0x3E)) & 0xFFFF);
    SetSW(0, (0xC - k) << 4);
    const unsigned angle = (RandCall() & 7) << 9;
    unsigned char* const x = Sc() + 0x34;
    SetSW(4, angle);
    int v = MH_CALL(Math_Sin)(static_cast<short>(angle));
    AddLong(x, static_cast<std::uint32_t>(Imul(v, SS(0)) >> 3));
    unsigned char* const z = Sc() + 0x38;
    v = MH_CALL(Math_Cos)(SS(4));
    AddLong(z, static_cast<std::uint32_t>(Imul(v, SS(0)) >> 3));
    Sc()[9] = 0;
    Sc()[0xA] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4A3F60: +0xA down by 2; at 0 the owner's +0xB down and MAGIC219's
// 0x4F6290 (the record's first five bytes cleared: a tail jump).
S07_EXPORT void __cdecl BonebreakMote_Fade(void) {
    AddB(Sc()[0xA], 0xFE);
    if (Sc()[0xA] != 0) return;
    Dec(Owner()[0xB]);
    Call0(bof3::addr::MagicFx_FreeCurrentRecord);
}

// original 0x4A3F90: a fan of 16 semi-transparent gouraud triangles round the
// screen point (+0x2E / +0x30), radius +9 x 2, the angle 0x100 a step; the
// centre shaded (+0xA x 12, x 11, x 4), the rim (x 12, x 8, x 4), bytes; each
// linked at the task on layer 2 behind a draw mode (tpage 0x35). The scratch
// (radius 0x903850, angle 0x903854, centre 0x903858 / 0x90385A) read again
// after every call.
S07_EXPORT void __cdecl BonebreakMote_DrawFan(void) {
    DrawMode(0x35);
    LinkAtSprite(2, 0xC);
    const unsigned char* const s = Sc();
    SetSW(4, 0);
    const unsigned a = s[0xA];
    const auto c0 = static_cast<unsigned char>(a * 12), c1 = static_cast<unsigned char>(a * 11),
               c2 = static_cast<unsigned char>(a << 2), c3 = static_cast<unsigned char>(a << 3);
    SetSW(0, static_cast<unsigned>(s[9]) << 1);
    SetSW(8, Word(s + 0x2E));
    SetSW(0xA, Word(s + 0x30));
    for (int i = 0; i < 0x10; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, SS(8));
        PutFloat(p + 0xC, SS(0xA));
        int v = MH_CALL(Math_Sin)(SS(4));
        PutFloat(p + 0x18, Mul12(v, SS(0)) + SS(8));
        v = MH_CALL(Math_Cos)(SS(4));
        PutFloat(p + 0x1C, Mul12(v, SS(0)) + SS(0xA));
        const unsigned next = static_cast<unsigned>(i + 1) << 8;
        SetSW(4, next);
        v = MH_CALL(Math_Sin)(static_cast<short>(next));
        PutFloat(p + 0x28, Mul12(v, SS(0)) + SS(8));
        v = MH_CALL(Math_Cos)(SS(4));
        PutFloat(p + 0x2C, Mul12(v, SS(0)) + SS(0xA));
        p[4] = c0;
        p[5] = c1;
        p[6] = c2;
        p[0x14] = c0;
        p[0x15] = c3;
        p[0x16] = c2;
        p[0x24] = c0;
        p[0x25] = c3;
        p[0x26] = c2;
        LinkAtSprite(2, 0x34);
    }
}

// original 0x4A4160: a ring of 16 semi-transparent gouraud quads round the
// screen point, from radius +9 x 2 (0x903850) to +9 x 3 (0x903852); the
// inner edge shaded (+0xA x 12, x 6, x 4), the outer 1; as the fan otherwise.
S07_EXPORT void __cdecl BonebreakMote_DrawRing(void) {
    DrawMode(0x35);
    LinkAtSprite(2, 0xC);
    const unsigned char* const s = Sc();
    SetSW(4, 0);
    const unsigned a = s[0xA];
    const auto c0 = static_cast<unsigned char>(a * 12), c1 = static_cast<unsigned char>(a * 6),
               c2 = static_cast<unsigned char>(a << 2);
    SetSW(0, static_cast<unsigned>(s[9]) << 1);
    SetSW(2, static_cast<unsigned>(s[9]) * 3);
    SetSW(8, Word(s + 0x2E));
    SetSW(0xA, Word(s + 0x30));
    for (int i = 0; i < 0x10; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        int v = MH_CALL(Math_Sin)(SS(4));
        PutFloat(p + 8, Mul12(v, SS(0)) + SS(8));
        v = MH_CALL(Math_Cos)(SS(4));
        PutFloat(p + 0xC, Mul12(v, SS(0)) + SS(0xA));
        v = MH_CALL(Math_Sin)(SS(4));
        PutFloat(p + 0x28, Mul12(v, SS(2)) + SS(8));
        v = MH_CALL(Math_Cos)(SS(4));
        PutFloat(p + 0x2C, Mul12(v, SS(2)) + SS(0xA));
        const unsigned next = static_cast<unsigned>(i + 1) << 8;
        SetSW(4, next);
        v = MH_CALL(Math_Sin)(static_cast<short>(next));
        PutFloat(p + 0x18, Mul12(v, SS(0)) + SS(8));
        v = MH_CALL(Math_Cos)(SS(4));
        PutFloat(p + 0x1C, Mul12(v, SS(0)) + SS(0xA));
        v = MH_CALL(Math_Sin)(SS(4));
        PutFloat(p + 0x38, Mul12(v, SS(2)) + SS(8));
        v = MH_CALL(Math_Cos)(SS(4));
        PutFloat(p + 0x3C, Mul12(v, SS(2)) + SS(0xA));
        p[4] = c0;
        p[5] = c1;
        p[6] = c2;
        p[0x14] = c0;
        p[0x15] = c1;
        p[0x16] = c2;
        for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[k] = 1;
        LinkAtSprite(2, 0x44);
    }
}

// original 0x4A43E0: the mote pool's alloc: the first of its 64 records with
// bit 0 clear gets it, its index in al; 0xFF when all are taken.
S07_EXPORT unsigned char __cdecl BonebreakMote_Alloc(void) {
    for (unsigned i = 0; i < 64; ++i) {
        unsigned char* const rec = PoolRecord(i);
        if (rec[0] & 1) continue;
        rec[0] |= 1;
        return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

// ===========================================================================
// MAGIC038 (row 121, War Shout read one id down)

namespace {

// The motes' orbit, one step: +0xC up, the position (the owner's, read again
// after each call) + sin / cos of (+0xC & 0x7F) sl 5 at radius sl 5 (the
// angle kept in dword 0x903850 and read again for the cos); +0x14 up, the
// height the owner's + sin((+0x14 & 0x3F) sl 6) x dword 0x903854 (0x800, read
// after the call).
void OrbitStep() {
    AddLong(Sc() + 0xC, 1);
    std::uint32_t angle = (static_cast<std::uint32_t>(Long(Sc() + 0xC)) & 0x7F) << 5;
    SetSD(0, angle);
    int v = MH_CALL(Math_Sin)(static_cast<int>(angle));
    SetLong(Sc() + 0x34, Sum(Shl(v, 0x11) >> 12, Long(Owner() + 0x34)));
    v = MH_CALL(Math_Cos)(SD(0));
    SetLong(Sc() + 0x38, Sum(Shl(v, 0x11) >> 12, Long(Owner() + 0x38)));
    AddLong(Sc() + 0x14, 1);
    angle = (static_cast<std::uint32_t>(Long(Sc() + 0x14)) & 0x3F) << 6;
    SetSD(4, 0x800);
    SetSD(0, angle);
    v = MH_CALL(Math_Sin)(static_cast<int>(angle));
    SetLong(Sc() + 0x3C, Sum(Imul(v, SD(4)), Long(Owner() + 0x3C)));
}

}  // namespace

// original 0x4A4440: the kind-2 task. A three-entry stack table by +1:
// WarShout_Start, _Rally, MagicFx_EndWhenChildrenDone.
S07_EXPORT void __cdecl WarShout_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::WarShout_Start, bof3::addr::WarShout_Rally,
                                                 bof3::addr::MagicFx_EndWhenChildrenDone};
    Dispatch(kPhases, 3, Sc()[1], "WarShout_Task");
}

// original 0x4A4470: the task to the side's centre, 0x200 up (+0x3C +
// 0x2000000); +0xB and +9 0, +1 on; sixteen motes (kind 1, 0x59): +0x80 this
// task (read after each create), +1 0, +0xB the number, +9 the delay number
// x 8 + 1; +0xB up for each. Row 26 of the CLUT strip back (32 words); row
// 2's first 16 words from the source with STP, then its word 0 without;
// Gfx_ClutStripDirty; sound 0x100.
S07_EXPORT void __cdecl WarShout_Start(void) {
    MH_CALL(MagicFx_CenterOnSide)();
    AddLong(Sc() + 0x3C, 0x2000000);
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    Inc(Sc()[1]);
    for (unsigned i = 0; i < 0x10; ++i) {
        const unsigned slot = NewTask(0x59);
        unsigned char* const self = Sc();
        unsigned char* const child = TaskSlot(slot);
        SetLong(child + 0x80, static_cast<std::int32_t>(Key(self)));
        child[1] = 0;
        child[0xB] = static_cast<unsigned char>(i);
        child[9] = static_cast<unsigned char>((i << 3) + 1);
        Inc(self[0xB]);
    }
    RestoreRow26();
    for (unsigned k = 0; k < 0x10; ++k) Gfx_ClutStrip[0x200 + k] = static_cast<unsigned short>(Gfx_ClutStripSource[0x200 + k] | 0x8000);
    Gfx_ClutStrip[0x200] = Gfx_ClutStripSource[0x200];
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4A4570: once +0xB (the motes alive) is 0: sound 0x101; one child
// (kind 1, 0x59) on every actor of the target's side (the enemies 3..10 with
// target bit 6, else the party 0..2) Battle_ActorIsOut does not answer for:
// +0x80 this task, +1 1, +3 the side's index, +4 2, its position the actor
// record's (read after the create); +0xB up for each; +1 on.
S07_EXPORT void __cdecl WarShout_Rally(void) {
    if (Sc()[0xB] != 0) return;
    MH_CALL(Sound_PlayById)(0x101);
    const bool enemies = (TargetByte() & 0x40) != 0;
    const unsigned count = enemies ? 8 : 3;
    for (unsigned i = 0; i < count; ++i) {
        if (MH_CALL(Battle_ActorIsOut)(enemies ? i + 3 : i) != 0) continue;
        const unsigned slot = NewTask(0x59);
        const unsigned char* const rec = enemies ? Mem(at::kEnemies + i * at::kEnemyStride) : PartyRecord(i);
        const std::int32_t x = Long(rec + 0x34);
        unsigned char* const self = Sc();
        unsigned char* const child = TaskSlot(slot);
        SetLong(child + 0x80, static_cast<std::int32_t>(Key(self)));
        child[1] = 1;
        child[3] = static_cast<unsigned char>(i);
        child[4] = 2;
        SetLong(child + 0x34, x);
        SetLong(child + 0x38, Long(rec + 0x38));
        SetLong(child + 0x3C, Long(rec + 0x3C));
        Inc(self[0xB]);
    }
    Inc(Sc()[1]);
}

// original 0x4A46F0: the children's kind-1 task, a jmp through
// WarShoutChild_Kinds (two entries: the mote, the buff) by +1, unchecked.
S07_EXPORT void __cdecl WarShoutChild_Task(void) {
    static constexpr std::uint32_t kKinds[2] = {bof3::addr::WarShoutMote_Run, bof3::addr::WarShoutBuff_Run};
    Dispatch(kKinds, 2, Sc()[1], "WarShoutChild_Task");
}

// original 0x4A4710: the frame-offset table 0x9039D8 the effects'
// (0x8E3580); a call through WarShoutMote_Steps (five entries: _Appear,
// _Rise, _Circle, _Fade, MAGIC058's 0x4AF490) by +2; with bytes +0 and +2 not
// 0 Sprite_UpdateScreen; the table the battle's again (0x8B3580).
S07_EXPORT void __cdecl WarShoutMote_Run(void) {
    static constexpr std::uint32_t kSteps[5] = {bof3::addr::WarShoutMote_Appear, bof3::addr::WarShoutMote_Rise,
                                                bof3::addr::WarShoutMote_Circle, bof3::addr::WarShoutMote_Fade,
                                                bof3::addr::MagicFx_UncountAndFree};
    const unsigned phase = Sc()[2];
    SetLong(Mem(kFrameSet), static_cast<std::int32_t>(kFrameSetEffect));
    Dispatch(kSteps, 5, phase, "WarShoutMote_Run");
    const unsigned char* const s = Sc();
    if (s[0] != 0 && s[2] != 0) MH_CALL(Sprite_UpdateScreen)();
    SetLong(Mem(kFrameSet), static_cast<std::int32_t>(kFrameSetBattle));
}

// original 0x4A4750: +9 down; at 0: +0xC and +0x14 0; the position the
// owner's + sin / cos(0) at radius sl 6 and the height + sin(0) sl 11 (the
// angles in dword 0x903850, the cos's read again); the sprite fields: +0x25
// 0x1D, +0x26 0, +0x27 0x20, +0x28 0, +0x24 0, +0 bit 5, +0x5C 1, +0x5D..+0x5F
// 0x80, +0x2A 0, +0x29 4, word +0x2C 0, +0x2B 0; its animation
// WarShoutMote_Animations[+0xB]; +2 on.
S07_EXPORT void __cdecl WarShoutMote_Appear(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    SetLong(Sc() + 0xC, 0);
    SetLong(Sc() + 0x14, 0);
    std::uint32_t angle = (static_cast<std::uint32_t>(Long(Sc() + 0xC)) & 0x7F) << 5;
    SetSD(0, angle);
    int v = MH_CALL(Math_Sin)(static_cast<int>(angle));
    SetLong(Sc() + 0x34, Sum(Shl(v, 0x12) >> 12, Long(Owner() + 0x34)));
    v = MH_CALL(Math_Cos)(SD(0));
    SetLong(Sc() + 0x38, Sum(Shl(v, 0x12) >> 12, Long(Owner() + 0x38)));
    angle = (static_cast<std::uint32_t>(Long(Sc() + 0x14)) & 0x3F) << 6;
    SetSD(0, angle);
    v = MH_CALL(Math_Sin)(static_cast<int>(angle));
    SetLong(Sc() + 0x3C, Sum(Shl(v, 0x17) >> 12, Long(Owner() + 0x3C)));
    unsigned char* const s = Sc();
    s[0x25] = 0x1D;
    s[0x26] = 0;
    s[0x27] = 0x20;
    s[0x28] = 0;
    s[0x24] = 0;
    s[0] |= 0x20;
    s[0x5C] = 1;
    s[0x5D] = 0x80;
    s[0x5E] = 0x80;
    s[0x5F] = 0x80;
    s[0x2A] = 0;
    s[0x29] = 4;
    SetWord(s + 0x2C, 0);
    s[0x2B] = 0;
    MH_CALL(Sprite_SetAnimation)(Mem(kWarShoutAnimations + s[0xB])[0]);
    Inc(Sc()[2]);
}

// original 0x4A48B0: the script ticked; the colour +0x5D..+0x5F up by 4; an
// orbit step; at +0x5D 0xC0 bit 5 of +0 cleared, +0x5C..+0x5F 0, +2 on.
S07_EXPORT void __cdecl WarShoutMote_Rise(void) {
    MH_CALL(Sprite_ScriptTick)();
    AddB(Sc()[0x5D], 4);
    AddB(Sc()[0x5E], 4);
    AddB(Sc()[0x5F], 4);
    OrbitStep();
    unsigned char* const s = Sc();
    if (s[0x5D] != 0xC0) return;
    s[0] &= 0xDF;
    s[0x5C] = 0;
    s[0x5D] = 0;
    s[0x5E] = 0;
    s[0x5F] = 0;
    Inc(s[2]);
}

// original 0x4A49D0: the script ticked; an orbit step; at +0xC 0x100 bit 5 of
// +0 set, +0x5C 1, +0x5D..+0x5F 0xC0, +2 on.
S07_EXPORT void __cdecl WarShoutMote_Circle(void) {
    MH_CALL(Sprite_ScriptTick)();
    OrbitStep();
    unsigned char* const s = Sc();
    if (Long(s + 0xC) != 0x100) return;
    s[0] |= 0x20;
    s[0x5C] = 1;
    s[0x5D] = 0xC0;
    s[0x5E] = 0xC0;
    s[0x5F] = 0xC0;
    Inc(s[2]);
}

// original 0x4A4AD0: the colour +0x5D..+0x5F down by 4; the script ticked; an
// orbit step; +0x5D 0x80; +2 on.
S07_EXPORT void __cdecl WarShoutMote_Fade(void) {
    AddB(Sc()[0x5D], 0xFC);
    AddB(Sc()[0x5E], 0xFC);
    AddB(Sc()[0x5F], 0xFC);
    MH_CALL(Sprite_ScriptTick)();
    OrbitStep();
    Sc()[0x5D] = 0x80;
    Inc(Sc()[2]);
}

// original 0x4A4BC0: the buff child, a jmp through WarShoutBuff_Steps (two
// entries: _Start, MagicFx_EndWithChildren) by +2, unchecked.
S07_EXPORT void __cdecl WarShoutBuff_Run(void) {
    static constexpr std::uint32_t kSteps[2] = {bof3::addr::WarShoutBuff_Start, bof3::addr::MagicFx_EndWithChildren};
    Dispatch(kSteps, 2, Sc()[2], "WarShoutBuff_Run");
}

// original 0x4A4BE0: MagicFx_BuffPopup(+4, +3) - kind 2, the side's index;
// +0xB 1, +2 on.
S07_EXPORT void __cdecl WarShoutBuff_Start(void) {
    const unsigned char* const s = Sc();
    MH_CALL(MagicFx_BuffPopup)(s[4], s[3]);
    Sc()[0xB] = 1;
    Inc(Sc()[2]);
}

// ===========================================================================
// MAGIC039 (row 78, Focus and Meditation read one id down)

// original 0x4A4C10: the kind-2 task. A three-entry stack table by +1:
// Focus_Start, MAGIC009's 0x49DA50 (+1 on at +0xB 0), BattleFx_Finish.
S07_EXPORT void __cdecl Focus_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::Focus_Start, bof3::addr::Chlorine_WaitChildren,
                                                 bof3::addr::BattleFx_Finish};
    Dispatch(kPhases, 3, Sc()[1], "Focus_Task");
}

// original 0x4A4C40: the position the owner's; +4 Focus_Kind (2, or 3 for
// ability 0xA3); +0xB 0, +1 on; the aura (kind 1, 0x3B): +0x80 this task
// (read after the create), +1 0, +4 this task's +4 - 2; then eight motes, the
// same with +1 1, +0xB the number, +9 FocusMote_Delays[i] + 9; +0xB up for
// each. Row 26 of the CLUT strip back; Gfx_ClutStripDirty; sound 0x100.
S07_EXPORT void __cdecl Focus_Start(void) {
    PositionFromOwner();
    const unsigned char kind = MH_AT(ByteFn, bof3::addr::Focus_Kind)();
    Sc()[4] = kind;
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
    {
        const unsigned slot = NewTask(0x3B);
        unsigned char* const self = Sc();
        unsigned char* const child = TaskSlot(slot);
        SetLong(child + 0x80, static_cast<std::int32_t>(Key(self)));
        child[1] = 0;
        child[4] = static_cast<unsigned char>(self[4] - 2);
        Inc(self[0xB]);
    }
    for (unsigned i = 0; i < 8; ++i) {
        const unsigned slot = NewTask(0x3B);
        unsigned char* const self = Sc();
        unsigned char* const child = TaskSlot(slot);
        SetLong(child + 0x80, static_cast<std::int32_t>(Key(self)));
        child[1] = 1;
        child[4] = static_cast<unsigned char>(self[4] - 2);
        const auto delay = static_cast<unsigned char>(Mem(kMoteDelays + i)[0] + 9);
        child[0xB] = static_cast<unsigned char>(i);
        child[9] = delay;
        Inc(self[0xB]);
    }
    RestoreRow26();
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4A4D90: 3 when the ability word 0x904B80 is 0xA3, else 2, in al
// (the rest of eax the caller's).
S07_EXPORT unsigned char __cdecl Focus_Kind(void) { return Word(Mem(kAbilityId)) == 0xA3 ? 3 : 2; }

// original 0x4A4DA0: the children's kind-1 task, a jmp through
// FocusChild_Kinds (two entries: the aura, a mote) by +1, unchecked.
S07_EXPORT void __cdecl FocusChild_Task(void) {
    static constexpr std::uint32_t kKinds[2] = {bof3::addr::FocusAura_Run, bof3::addr::FocusMote_Run};
    Dispatch(kKinds, 2, Sc()[1], "FocusChild_Task");
}

// original 0x4A4DC0: a six-entry stack table by +2 - _Start, _Grow, _Swell,
// _Brighten, _Dim, _End - then with +0 bit 0 and +2 not 0: the actor matrix
// pushed, the ring and the disc, the matrix popped.
S07_EXPORT void __cdecl FocusAura_Run(void) {
    static constexpr std::uint32_t kSteps[6] = {bof3::addr::FocusAura_Start,    bof3::addr::FocusAura_Grow,
                                                bof3::addr::FocusAura_Swell,    bof3::addr::FocusAura_Brighten,
                                                bof3::addr::FocusAura_Dim,      bof3::addr::FocusAura_End};
    Dispatch(kSteps, 6, Sc()[2], "FocusAura_Run");
    const unsigned char* const s = Sc();
    if ((s[0] & 1) == 0 || s[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::FocusAura_DrawRing);
    Call0(bof3::addr::FocusAura_DrawDisc);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4A4E30: the position the owner's; +9 0, +0xA 1, +2 on.
S07_EXPORT void __cdecl FocusAura_Start(void) {
    PositionFromOwner();
    Sc()[9] = 0;
    Sc()[0xA] = 1;
    Inc(Sc()[2]);
}

// original 0x4A4E80: +9 up by 2; at 0x10 +0xA 0 and +2 on.
S07_EXPORT void __cdecl FocusAura_Grow(void) {
    AddB(Sc()[9], 2);
    unsigned char* const s = Sc();
    if (s[9] != 0x10) return;
    s[0xA] = 0;
    Inc(s[2]);
}

// original 0x4A4EB0: +9 up, +0xA up by 2; at 0x18 the source sprite's tint
// released and a black one set (Sprite_SetTint(source, 0, 0, 0, 1), the
// source read again), its slot to +0xB; +2 on.
S07_EXPORT void __cdecl FocusAura_Swell(void) {
    Inc(Sc()[9]);
    AddB(Sc()[0xA], 2);
    if (Sc()[0xA] != 0x18) return;
    MH_CALL(Sprite_ReleaseTint)(Source());
    const unsigned char tint = MH_CALL(Sprite_SetTint)(Source(), 0, 0, 0, 1);
    Sc()[0xB] = tint;
    Inc(Sc()[2]);
}

// original 0x4A4F10: +9 up; the tint record +0xB (MoveScript_TintRecords + 12
// n) up by 2 in its three channels (+2..+4); at channel +2 0x10 +2 on.
S07_EXPORT void __cdecl FocusAura_Brighten(void) {
    Inc(Sc()[9]);
    unsigned char* const s = Sc();
    for (unsigned c = 2; c < 5; ++c) AddB(MoveScript_TintRecords[s[0xB] * 12u + c], 2);
    if (MoveScript_TintRecords[s[0xB] * 12u + 2] == 0x10) Inc(s[2]);
}

// original 0x4A4F90: +9 up; the tint record's three channels down by 2; at
// channel +2 0 +2 on.
S07_EXPORT void __cdecl FocusAura_Dim(void) {
    Inc(Sc()[9]);
    unsigned char* const s = Sc();
    for (unsigned c = 2; c < 5; ++c) AddB(MoveScript_TintRecords[s[0xB] * 12u + c], 0xFE);
    if (MoveScript_TintRecords[s[0xB] * 12u + 2] == 0) Inc(s[2]);
}

// original 0x4A5010: +9 up, +0xA down by 2; at 0 the source's tint released,
// the target flashed, the owner's +0xB down, the task freed (a tail jump).
S07_EXPORT void __cdecl FocusAura_End(void) {
    Inc(Sc()[9]);
    AddB(Sc()[0xA], 0xFE);
    if (Sc()[0xA] != 0) return;
    MH_CALL(Sprite_ReleaseTint)(Source());
    MH_CALL(BattleActor_Flash)(TargetByte());
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4A5060: a three-entry stack table by +2 - _Start, _Grow, _Rise -
// then with +0 bit 0 and +2 not 0 the screen point and the mote's quad.
S07_EXPORT void __cdecl FocusMote_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::FocusMote_Start, bof3::addr::FocusMote_Grow,
                                                bof3::addr::FocusMote_Rise};
    Dispatch(kSteps, 3, Sc()[2], "FocusMote_Run");
    const unsigned char* const s = Sc();
    if ((s[0] & 1) == 0 || s[2] == 0) return;
    MH_CALL(BattleActor_UpdateScreenXY)();
    Call0(bof3::addr::FocusMote_Draw);
}

// original 0x4A50B0: +9 down; at 0 the angle +0xB sl 9 (word 0x90385A, read
// again for the sin); the position the owner's + cos / sin x 3 sl 2; +9 0,
// +0xA 4, +2 on.
S07_EXPORT void __cdecl FocusMote_Start(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    const unsigned angle = (static_cast<unsigned>(Sc()[0xB]) << 9) & 0xFFFF;
    SetSW(0xA, angle);
    int v = MH_CALL(Math_Cos)(static_cast<short>(angle));
    SetLong(Sc() + 0x34, Sum(Shl(Imul(v, 3), 0xE) >> 12, Long(Owner() + 0x34)));
    v = MH_CALL(Math_Sin)(SS(0xA));
    SetLong(Sc() + 0x38, Sum(Shl(Imul(v, 3), 0xE) >> 12, Long(Owner() + 0x38)));
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
    Sc()[9] = 0;
    Sc()[0xA] = 4;
    Inc(Sc()[2]);
}

// original 0x4A5160: +9 up by 0x10; at 0x80 +2 on.
S07_EXPORT void __cdecl FocusMote_Grow(void) {
    AddB(Sc()[9], 0x10);
    if (Sc()[9] == 0x80) Inc(Sc()[2]);
}

// original 0x4A5180: +9 up by 8, +0xA down; at 0 the owner's +0xB down and the
// task freed (a tail jump).
S07_EXPORT void __cdecl FocusMote_Rise(void) {
    AddB(Sc()[9], 8);
    Dec(Sc()[0xA]);
    if (Sc()[0xA] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4A51C0: one semi-transparent gouraud quad over the screen point,
// half-width +0xA and height +9 above it (scratch words 0x903858 / 0x903856,
// the point at 0x90385A / 0x90385C), shaded 1 at the top and
// FocusMote_Shades[+4] (words 0x903850 / 52 / 54) at the bottom; linked at
// the task on layer 0 behind a draw mode (tpage 0x35).
S07_EXPORT void __cdecl FocusMote_Draw(void) {
    const unsigned char* const s = Sc();
    SetSW(6, s[9]);
    SetSW(8, s[0xA]);
    SetSW(0xA, Word(s + 0x2E));
    SetSW(0xC, Word(s + 0x30));
    const unsigned k = s[4] * 3u;
    SetSW(0, Mem(kMoteShades + k)[0]);
    SetSW(2, Mem(kMoteShades + 1 + k)[0]);
    SetSW(4, Mem(kMoteShades + 2 + k)[0]);
    DrawMode(0x35);
    LinkAtSprite(0, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyG4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    PutFloat(p + 8, SS(0xA) - SS(8));
    PutFloat(p + 0xC, SS(0xC) - SS(6));
    PutFloat(p + 0x18, SS(0xA) + SS(8));
    PutFloat(p + 0x1C, SS(0xC) - SS(6));
    PutFloat(p + 0x28, SS(0xA) - SS(8));
    PutFloat(p + 0x2C, SS(0xC));
    PutFloat(p + 0x38, SS(0xA) + SS(8));
    PutFloat(p + 0x3C, SS(0xC));
    for (unsigned c : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u}) p[c] = 1;
    p[0x24] = SB(0);
    p[0x25] = SB(2);
    p[0x26] = SB(4);
    p[0x34] = SB(0);
    p[0x35] = SB(2);
    p[0x36] = SB(4);
    LinkAtSprite(0, 0x44);
}

namespace {

// The ring's shades, words 0x903850 / 52 / 54: FocusAura_RingShades[+4]
// (or FocusAura_DiscShades) x +9 below 0x10, else x 16.
void Shades(std::uint32_t table) {
    const unsigned char* const s = Sc();
    const unsigned k = s[4] * 3u;
    if (s[9] < 0x10) {
        SetSW(0, Mem(table + k)[0] * static_cast<unsigned>(s[9]));
        SetSW(2, Mem(table + 1 + k)[0] * static_cast<unsigned>(s[9]));
        SetSW(4, Mem(table + 2 + k)[0] * static_cast<unsigned>(s[9]));
    } else {
        SetSW(0, static_cast<unsigned>(Mem(table + k)[0]) << 4);
        SetSW(2, static_cast<unsigned>(Mem(table + 1 + k)[0]) << 4);
        SetSW(4, static_cast<unsigned>(Mem(table + 2 + k)[0]) << 4);
    }
}

}  // namespace

// original 0x4A53A0: the aura's ring, 64 semi-transparent gouraud quads under
// the actor matrix. Height word 0x903858 = +0xA; each step an angle
// ((Rand & 3) + +9 + step) & 0xF sl 8 gives the inner lift (0x903856) and
// radius (0x90385E), the outer radius 0x80 (0x90385C). Each quad: the last
// inner and outer points and the new ones at angle step x 0x40 (vertices
// 0x9037A0..B8), projected (Gte_RotTransPers4), depth-sorted, and linked at
// the task's +0x34 / +0x38 each moved by the new outer x sl 9 (both by x) on
// layer 0; shaded 1 inside, FocusAura_RingShades outside.
S07_EXPORT void __cdecl FocusAura_DrawRing(void) {
    SetSW(8, Sc()[0xA]);
    std::uint32_t r = RandCall();
    unsigned angle = (((r & 3) + Sc()[9]) & 0xF) << 8;
    SetSW(0xA, angle);
    {
        int v = MH_CALL(Math_Sin)(static_cast<short>(angle));
        const int inner = Shl(Imul(v, SS(8)), 1) >> 12;
        const short again = SS(0xA);
        const std::uint32_t lift = static_cast<std::uint32_t>(inner) - 6u * static_cast<std::uint32_t>(SD(8));
        SetSW(0xC, 0x80);
        SetSW(6, lift);
        v = MH_CALL(Math_Sin)(again);
        const int outer = Shl(Imul(v, SS(8)), 2) >> 12;
        SetSW(0xE, 8u * static_cast<std::uint32_t>(SD(8)) + 0x80u - static_cast<std::uint32_t>(outer));
    }
    Shades(kRingShades);
    int v = MH_CALL(Math_Cos)(0);
    SetVW(8, static_cast<unsigned>(Mul12(v, SS(0xE))));
    v = MH_CALL(Math_Sin)(0);
    SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(0xE))));
    SetVW(0xC, SW(6));
    v = MH_CALL(Math_Cos)(0);
    SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(0xC))));
    v = MH_CALL(Math_Sin)(0);
    SetVW(0x14, 0);
    SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(0xC))));
    SetVW(0x1C, 0);
    for (unsigned step = 1, e = 0x40; e < 0x1040; ++step, e += 0x40) {
        r = RandCall();
        angle = (((r & 3) + Sc()[9] + step) & 0xF) << 8;
        SetSW(0xA, angle);
        v = MH_CALL(Math_Sin)(static_cast<short>(angle));
        const int inner = Shl(Imul(v, SS(8)), 1) >> 12;
        const short again = SS(0xA);
        SetSW(6, static_cast<std::uint32_t>(inner) - 6u * static_cast<std::uint32_t>(SD(8)));
        v = MH_CALL(Math_Sin)(again);
        const int outer = Shl(Imul(v, SS(8)), 2) >> 12;
        const std::uint32_t radius = 8u * static_cast<std::uint32_t>(SD(8)) + 0x80u - static_cast<std::uint32_t>(outer);
        const std::uint16_t y1 = VW(0xA), x1 = VW(8);
        SetSW(0xE, radius);
        const std::uint16_t z1 = VW(0xC);
        SetVW(0, x1);
        SetVW(2, y1);
        SetVW(4, z1);
        v = MH_CALL(Math_Cos)(static_cast<int>(e));
        SetVW(8, static_cast<unsigned>(Mul12(v, SS(0xE))));
        v = MH_CALL(Math_Sin)(static_cast<int>(e));
        const int y = Mul12(v, SS(0xE));
        const std::uint16_t x3 = VW(0x18), y3 = VW(0x1A);
        SetVW(0xA, static_cast<unsigned>(y));
        SetVW(0xC, SW(6));
        SetVW(0x10, x3);
        SetVW(0x12, y3);
        v = MH_CALL(Math_Cos)(static_cast<int>(e));
        SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(0xC))));
        v = MH_CALL(Math_Sin)(static_cast<int>(e));
        const int y4 = Mul12(v, SS(0xC));
        const unsigned char* const s = Sc();
        SetVW(0x1A, static_cast<unsigned>(y4));
        const std::uint32_t shift = static_cast<std::uint32_t>(Shl(VS(0x18), 9));
        const auto x = static_cast<unsigned long>(static_cast<std::uint32_t>(Long(s + 0x34)) + shift);
        const auto z = static_cast<unsigned long>(static_cast<std::uint32_t>(Long(s + 0x38)) + shift);
        DrawMode(0x35);
        MH_CALL(MapView_LinkPrimAt)(x, z, 0, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        for (unsigned c : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u}) p[c] = 1;
        p[0x24] = SB(0);
        p[0x25] = SB(2);
        p[0x26] = SB(4);
        p[0x34] = SB(0);
        p[0x35] = SB(2);
        p[0x36] = SB(4);
        long depth, flag;
        S07_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), p + 8, p + 0x18, p + 0x28, p + 0x38, &depth,
                                         &flag);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        MH_CALL(MapView_LinkPrimAt)(x, z, 0, 0x44);
    }
}

// original 0x4A57C0: the aura's disc, 16 semi-transparent gouraud triangles
// under the actor matrix: the centre (0, 0, 0) and two rim points of radius
// 0x88 (word 0x903856) 0x100 apart, projected (Gte_RotTransPers3),
// depth-sorted and committed to layer 5; shaded 1 at the centre,
// FocusAura_DiscShades at the rim; between draw modes (tpage 0x35, then
// 0x15).
S07_EXPORT void __cdecl FocusAura_DrawDisc(void) {
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    SetSW(6, 0x88);
    Shades(kDiscShades);
    int v = MH_CALL(Math_Sin)(0);
    SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(6))));
    v = MH_CALL(Math_Cos)(0);
    SetVW(0x14, 0);
    SetVW(0x12, static_cast<unsigned>(Mul12(v, SS(6))));
    SetVW(0xC, 0);
    SetVW(4, 0);
    for (int e = 0x100; e < 0x1100; e += 0x100) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const std::uint16_t x2 = VW(0x10), y2 = VW(0x12);
        SetVW(0, 0);
        SetVW(2, 0);
        SetVW(8, x2);
        SetVW(0xA, y2);
        v = MH_CALL(Math_Sin)(e);
        SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(6))));
        v = MH_CALL(Math_Cos)(e);
        SetVW(0x12, static_cast<unsigned>(Mul12(v, SS(6))));
        long depth, flag;
        S07_AS(Rtp3Fn, Gte_RotTransPers3)(VP(0), VP(8), VP(0x10), p + 8, p + 0x18, p + 0x28, &depth, &flag);
        MH_CALL(Gte_PrimDepths3_10B)(p);
        p[4] = 1;
        p[5] = 1;
        p[6] = 1;
        p[0x14] = SB(0);
        p[0x15] = SB(2);
        p[0x16] = SB(4);
        p[0x24] = SB(0);
        p[0x25] = SB(2);
        p[0x26] = SB(4);
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// ===========================================================================
// MAGIC040 (row 79, Enlighten read one id down)

// original 0x4A5A10: the kind-2 task. A three-entry stack table by +1:
// Enlighten_Start, _Apply, MagicFx_EndWhenChildrenDone.
S07_EXPORT void __cdecl Enlighten_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::Enlighten_Start, bof3::addr::Enlighten_Apply,
                                                 bof3::addr::MagicFx_EndWhenChildrenDone};
    Dispatch(kPhases, 3, Sc()[1], "Enlighten_Task");
}

// original 0x4A5A40: the owner's direction +8 and position; +0xB 0, +1 on;
// two children (kind 1, 0x3C): the rays (+1 0) and the ring (+1 1), each
// +0x80 this task (read after the create) and +0xB up. Row 26 of the CLUT
// strip back; Gfx_ClutStripDirty; sound 0x100.
S07_EXPORT void __cdecl Enlighten_Start(void) {
    Sc()[8] = Owner()[8];
    PositionFromOwner();
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
    for (unsigned kind = 0; kind < 2; ++kind) {
        const unsigned slot = NewTask(0x3C);
        unsigned char* const self = Sc();
        unsigned char* const child = TaskSlot(slot);
        SetLong(child + 0x80, static_cast<std::int32_t>(Key(self)));
        child[1] = static_cast<unsigned char>(kind);
        Inc(self[0xB]);
    }
    RestoreRow26();
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4A5B50: once +0xB (the children alive) is 0: the buff
// (MagicFx_ApplyBuff(3, the target byte)) and its popup (kind 1, 0x48, the
// BuffPopup task): +0x80 this task (read after the create), +4 its icon (3,
// or 8 when resisted), +9 1, +0xA 0; +0xB up, +1 on.
S07_EXPORT void __cdecl Enlighten_Apply(void) {
    if (Sc()[0xB] != 0) return;
    const unsigned char applied = MH_CALL(MagicFx_ApplyBuff)(3, TargetByte());
    const unsigned slot = NewTask(0x48);
    unsigned char* const self = Sc();
    unsigned char* const child = TaskSlot(slot);
    SetLong(child + 0x80, static_cast<std::int32_t>(Key(self)));
    child[4] = applied != 0 ? 3 : 8;
    child[9] = 1;
    child[0xA] = 0;
    Inc(self[0xB]);
    Inc(Sc()[1]);
}

// original 0x4A5C10: the children's kind-1 task, a jmp through
// EnlightenChild_Kinds (two entries: the rays, the ring) by +1, unchecked.
S07_EXPORT void __cdecl EnlightenChild_Task(void) {
    static constexpr std::uint32_t kKinds[2] = {bof3::addr::EnlightenRays_Run, bof3::addr::EnlightenRing_Run};
    Dispatch(kKinds, 2, Sc()[1], "EnlightenChild_Task");
}

// original 0x4A5C30: a four-entry stack table by +2 - _Start, _Grow, group
// S20's Magic088_WaveGrow, _Fade - then with +0 bit 0 and +2 not 0: the disc,
// and the lines twice: from angle step +9 at length +0xA x 3, from +9 + 4 at
// +0xA x 2 (each byte of the task read again after the call before).
S07_EXPORT void __cdecl EnlightenRays_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::EnlightenRays_Start, bof3::addr::EnlightenRays_Grow,
                                                bof3::addr::Magic088_WaveGrow, bof3::addr::EnlightenRays_Fade};
    Dispatch(kSteps, 4, Sc()[2], "EnlightenRays_Run");
    const unsigned char* s = Sc();
    if ((s[0] & 1) == 0 || s[2] == 0) return;
    Call0(bof3::addr::EnlightenRays_DrawDisc);
    s = Sc();
    MH_AT(Fn2, bof3::addr::EnlightenRays_DrawLines)(s[9], s[0xA] * 3u);
    s = Sc();
    MH_AT(Fn2, bof3::addr::EnlightenRays_DrawLines)(static_cast<unsigned char>(s[9] + 4), s[0xA] * 2u);
}

// original 0x4A5CB0: the owner's direction and position; anchored on the
// target (Enlighten_AnchorToActor); +9 and +0xA 0, +2 on.
S07_EXPORT void __cdecl EnlightenRays_Start(void) {
    Sc()[8] = Owner()[8];
    PositionFromOwner();
    Call0(bof3::addr::Enlighten_AnchorToActor);
    Sc()[9] = 0;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
}

// original 0x4A5D20: +9 up, +0xA up; at +0xA 0x10 +2 on.
S07_EXPORT void __cdecl EnlightenRays_Grow(void) {
    Inc(Sc()[9]);
    Inc(Sc()[0xA]);
    if (Sc()[0xA] == 0x10) Inc(Sc()[2]);
}

// original 0x4A5D50: +9 up, +0xA down; at 0 the owner's +0xB down and the
// task freed (a tail jump).
S07_EXPORT void __cdecl EnlightenRays_Fade(void) {
    Inc(Sc()[9]);
    Dec(Sc()[0xA]);
    if (Sc()[0xA] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4A5D90: a two-entry stack table by +2 - _Start, _Spread - then
// with +0 bit 0 and +2 not 0 the ring.
S07_EXPORT void __cdecl EnlightenRing_Run(void) {
    static constexpr std::uint32_t kSteps[2] = {bof3::addr::EnlightenRing_Start, bof3::addr::EnlightenRing_Spread};
    Dispatch(kSteps, 2, Sc()[2], "EnlightenRing_Run");
    const unsigned char* const s = Sc();
    if ((s[0] & 1) == 0 || s[2] == 0) return;
    Call0(bof3::addr::EnlightenRing_Draw);
}

// original 0x4A5DD0: the owner's direction and position; anchored on the
// target; +0xB 0xB, +9 0, +0xA 0x80, +2 on.
S07_EXPORT void __cdecl EnlightenRing_Start(void) {
    Sc()[8] = Owner()[8];
    PositionFromOwner();
    Call0(bof3::addr::Enlighten_AnchorToActor);
    Sc()[0xB] = 0xB;
    Sc()[9] = 0;
    Sc()[0xA] = 0x80;
    Inc(Sc()[2]);
}

// original 0x4A5E40: +0xB down, +9 up by it, +0xA down by 0xC; at +0xB 0 the
// owner's +0xB down and the task freed (a tail jump).
S07_EXPORT void __cdecl EnlightenRing_Spread(void) {
    Dec(Sc()[0xB]);
    unsigned char* const s = Sc();
    AddB(s[9], s[0xB]);
    AddB(s[0xA], 0xF4);
    if (Sc()[0xB] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4A5E90: the task's screen point (BattleActor_UpdateScreenXY);
// then for a party target (the target byte below 3) moved by that member's
// offset: the index 0x904B89 through Enlighten_AnchorIndexB into
// Enlighten_AnchorOffsetsB while the record's +0x134 has bit 1, else its
// +0x89 into Enlighten_AnchorOffsets; the pair (+8 >> 1) + index x 2: dx
// added to word +0x2E for a direction 0 or 3, else taken off; dy added to
// +0x30.
S07_EXPORT void __cdecl Enlighten_AnchorToActor(void) {
    MH_CALL(BattleActor_UpdateScreenXY)();
    const unsigned target = TargetByte();
    if (target >= 3) return;
    const unsigned char* const rec = PartyRecord(target);
    unsigned index;
    std::uint32_t table;
    if (rec[0x134] & 2) {
        index = Mem(kAnchorIndexB + Mem(kFormIndex)[0])[0];
        table = kAnchorOffsetsB;
    } else {
        index = rec[0x89];
        table = kAnchorOffsets;
    }
    unsigned char* const s = Sc();
    const unsigned facing = s[8];
    const std::uint16_t dx = Word(Mem(table + ((facing >> 1) + index * 2) * 4));
    if (facing == 0 || facing == 3) SetWord(s + 0x2E, (Word(s + 0x2E) + dx) & 0xFFFF);
    else SetWord(s + 0x2E, (Word(s + 0x2E) - dx) & 0xFFFF);
    unsigned char* const t = Sc();
    const std::uint16_t dy = Word(Mem(table + 2 + ((t[8] >> 1) + index * 2) * 4));
    SetWord(t + 0x30, (Word(t + 0x30) + dy) & 0xFFFF);
}

// original 0x4A5FC0: the rays: from the screen point (+0x2E / +0x30, kept in
// words 0x903850 / 52) semi-transparent gouraud lines at angle (step & 0x1F)
// sl 7 (word 0x903858, read again for each call) and length `length` (its low
// word), for step = `first` (a byte), + 8, .. while the byte step stays below
// first + 0x20 - four lines; for a first of 0xE0 or more the byte never gets
// there and the loop does not end (the original's; section 7 of the doc).
// Shaded (0x80, 0x70, 0x60) at the point, 1 at the end; committed to layer 3
// between draw modes (tpage 0x35, then 0x15).
S07_EXPORT void __cdecl EnlightenRays_DrawLines(unsigned first, unsigned length) {
    const unsigned char* const s = Sc();
    SetSW(0, Word(s + 0x2E));
    SetSW(2, Word(s + 0x30));
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
    auto step = static_cast<unsigned char>(first);
    const unsigned end = static_cast<unsigned>(step) + 0x20;
    const int len = static_cast<int>(length & 0xFFFF);
    do {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetLineG2)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const short x = SS(0);
        SetSW(8, static_cast<unsigned>(step & 0x1F) << 7);
        PutFloat(p + 8, x);
        PutFloat(p + 0xC, SS(2));
        int v = MH_CALL(Math_Cos)(SS(8));
        PutFloat(p + 0x18, Mul12(v, len) + SS(0));
        v = MH_CALL(Math_Sin)(SS(8));
        PutFloat(p + 0x1C, Mul12(v, len) + SS(2));
        p[4] = 0x80;
        p[5] = 0x70;
        p[6] = 0x60;
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        MH_CALL(Gfx_CommitPrim)(3, 0x24);
        step = static_cast<unsigned char>(step + 8);
    } while (step < end);
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
}

// original 0x4A6120: a disc of 16 semi-transparent gouraud triangles round the
// screen point, radius +0xA (word 0x903854), 0x200 apart; shaded 0x80 at the
// centre, 1 at the rim; committed to layer 3 between draw modes (tpage 0x35,
// then 0x15).
S07_EXPORT void __cdecl EnlightenRays_DrawDisc(void) {
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
    const unsigned char* const s = Sc();
    SetSW(4, s[0xA]);
    SetSW(0, Word(s + 0x2E));
    SetSW(2, Word(s + 0x30));
    for (int e = 0; e < 0x2000;) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, SS(0));
        PutFloat(p + 0xC, SS(2));
        int v = MH_CALL(Math_Sin)(e);
        PutFloat(p + 0x18, Mul12(v, SS(4)) + SS(0));
        v = MH_CALL(Math_Cos)(e);
        PutFloat(p + 0x1C, Mul12(v, SS(4)) + SS(2));
        e += 0x200;
        v = MH_CALL(Math_Sin)(e);
        PutFloat(p + 0x28, Mul12(v, SS(4)) + SS(0));
        v = MH_CALL(Math_Cos)(e);
        PutFloat(p + 0x2C, Mul12(v, SS(4)) + SS(2));
        p[4] = 0x80;
        p[5] = 0x80;
        p[6] = 0x80;
        for (unsigned c : {0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[c] = 1;
        MH_CALL(Gfx_CommitPrim)(3, 0x34);
    }
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
}

// original 0x4A62B0: a ring of 32 semi-transparent flat lines round the
// screen point, radius +9 (word 0x903854), 0x80 apart (word 0x903858, read
// again for each cos), shaded +0xA grey (word 0x90385A); committed to layer 3
// between draw modes (tpage 0x35, then 0x15).
S07_EXPORT void __cdecl EnlightenRing_Draw(void) {
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
    const unsigned char* const s = Sc();
    SetSW(0, Word(s + 0x2E));
    SetSW(2, Word(s + 0x30));
    SetSW(0xA, s[0xA]);
    SetSW(4, s[9]);
    for (unsigned i = 0; i < 0x20; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetLineF2)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        unsigned angle = (i << 7) & 0xFFFF;
        SetSW(8, angle);
        int v = MH_CALL(Math_Sin)(static_cast<short>(angle));
        PutFloat(p + 8, Mul12(v, SS(4)) + SS(0));
        v = MH_CALL(Math_Cos)(SS(8));
        PutFloat(p + 0xC, Mul12(v, SS(4)) + SS(2));
        angle = ((i + 1) << 7) & 0xFFFF;
        SetSW(8, angle);
        v = MH_CALL(Math_Sin)(static_cast<short>(angle));
        PutFloat(p + 0x14, Mul12(v, SS(4)) + SS(0));
        v = MH_CALL(Math_Cos)(SS(8));
        PutFloat(p + 0x18, Mul12(v, SS(4)) + SS(2));
        p[4] = SB(0xA);
        p[5] = SB(0xA);
        p[6] = SB(0xA);
        MH_CALL(Gfx_CommitPrim)(3, 0x20);
    }
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
}

void MagicS07_Inject() {
    if (bof3::WantsShadow("magic_s07")) magic_s07::SelfTest();
    BOF3_INJECT(Bonebreak_Task);
    BOF3_INJECT(Bonebreak_Start);
    BOF3_INJECT(BonebreakChild_Task);
    BOF3_INJECT(BonebreakChild_Run);
    BOF3_INJECT(BonebreakChild_Burst);
    BOF3_INJECT(BonebreakChild_WaitScript);
    BOF3_INJECT(BonebreakChild_Shade);
    BOF3_INJECT(BonebreakChild_WaitMotes);
    BOF3_INJECT(BonebreakMote_Task);
    BOF3_INJECT(BonebreakMote_Run);
    BOF3_INJECT(BonebreakMote_Launch);
    BOF3_INJECT(BonebreakMote_Fade);
    BOF3_INJECT(BonebreakMote_DrawFan);
    BOF3_INJECT(BonebreakMote_DrawRing);
    BOF3_INJECT(BonebreakMote_Alloc);
    BOF3_INJECT(WarShout_Task);
    BOF3_INJECT(WarShout_Start);
    BOF3_INJECT(WarShout_Rally);
    BOF3_INJECT(WarShoutChild_Task);
    BOF3_INJECT(WarShoutMote_Run);
    BOF3_INJECT(WarShoutMote_Appear);
    BOF3_INJECT(WarShoutMote_Rise);
    BOF3_INJECT(WarShoutMote_Circle);
    BOF3_INJECT(WarShoutMote_Fade);
    BOF3_INJECT(WarShoutBuff_Run);
    BOF3_INJECT(WarShoutBuff_Start);
    BOF3_INJECT(Focus_Task);
    BOF3_INJECT(Focus_Start);
    BOF3_INJECT(Focus_Kind);
    BOF3_INJECT(FocusChild_Task);
    BOF3_INJECT(FocusAura_Run);
    BOF3_INJECT(FocusAura_Start);
    BOF3_INJECT(FocusAura_Grow);
    BOF3_INJECT(FocusAura_Swell);
    BOF3_INJECT(FocusAura_Brighten);
    BOF3_INJECT(FocusAura_Dim);
    BOF3_INJECT(FocusAura_End);
    BOF3_INJECT(FocusMote_Run);
    BOF3_INJECT(FocusMote_Start);
    BOF3_INJECT(FocusMote_Grow);
    BOF3_INJECT(FocusMote_Rise);
    BOF3_INJECT(FocusMote_Draw);
    BOF3_INJECT(FocusAura_DrawRing);
    BOF3_INJECT(FocusAura_DrawDisc);
    BOF3_INJECT(Enlighten_Task);
    BOF3_INJECT(Enlighten_Start);
    BOF3_INJECT(Enlighten_Apply);
    BOF3_INJECT(EnlightenChild_Task);
    BOF3_INJECT(EnlightenRays_Run);
    BOF3_INJECT(EnlightenRays_Start);
    BOF3_INJECT(EnlightenRays_Grow);
    BOF3_INJECT(EnlightenRays_Fade);
    BOF3_INJECT(EnlightenRing_Run);
    BOF3_INJECT(EnlightenRing_Start);
    BOF3_INJECT(EnlightenRing_Spread);
    BOF3_INJECT(Enlighten_AnchorToActor);
    BOF3_INJECT(EnlightenRays_DrawLines);
    BOF3_INJECT(EnlightenRays_DrawDisc);
    BOF3_INJECT(EnlightenRing_Draw);
}

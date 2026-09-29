// Four enemy-skill overlays compiled into the exe, round nine group C2
// (docs/magic_c2.md): the PSX's MAGIC057, MAGIC081, MAGIC116 and
// MAGIC129.EMI, Magic_Rows rows 84, 85, 59 and 145. Read one id down
// (docs/cut-content.md section 2) they are TCRF's Bone Dance, RottenBreath,
// UtmostAttack and Holocaust, which TCRF lists among the enemy-only skills;
// the names below use those labels as hypotheses and say what the code does.
//
//   - MAGIC057 0x4AE7C0..0x4AF035: the caster's animation 3, a camera shake of
//     three beats, then 32 falling bones (kind-1 tasks 0x41) that land on the
//     map's elevation, and a follower kind no creator here starts;
//   - MAGIC081 0x4BF8D0..0x4C01E6: a cloud task that emits up to 48 motes
//     (the pool 0x68E1B8) which swirl and fly at a task kept on the target
//     side's centre, each a textured quad;
//   - MAGIC116 0x4D9AE0..0x4D9F35: 0x78 frames of streaks, eight a frame from a
//     pool of 128 (0x698EC0), each a gouraud quad between two moving points
//     projected through Capcom's unnamed 0x494110;
//   - MAGIC129 0x4E3260..0x4E4416: a beam from the acting actor's enemy record
//     to each party member present, drawn as a band of gouraud quads, and
//     sparks (the pool 0x6A2D38) rising round it.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// stack table aborts where the original would call through whatever follows it
// (docs/magic_fx_reached.md section 3, the precedent). The .data dispatches are
// read in place and unchecked, as the originals read them.
#include "game/magic_c2.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;
namespace addr = bof3::addr;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// The scratch the overlays keep their working values in: DamageScratch's
// sixteen bytes (0x903850..0x90385F, words by function) and the first two
// words of Prim_VertexScratch (0x9037A0). Both are read again after every
// call, as the originals read them.
constexpr std::uint32_t kS = 0x903850;
constexpr std::uint32_t kV = 0x9037A0;
// The sprite frame-offset table pointer (magic_s17.cpp's kFrameSet): the
// bones switch it to 0x8E3580 around their phase and put back 0x8B3580.
constexpr std::uint32_t kFrameSet = 0x9039D8;
// Row 26 of Gfx_ClutStrip and of its source, 0x4000 bytes below.
constexpr std::uint32_t kClutRow26 = 0x812980;
constexpr std::uint32_t kClutRow26Source = 0x80E980;
// The overlays' own .data and .bss.
constexpr std::uint32_t kShakeCounts = 0x65AA64;      // BoneDanceShake_Counts: four bytes by +9
constexpr std::uint32_t kRottenTypes = 0x65B358;      // RottenBreathChild_Types: two entries by +1
constexpr std::uint32_t kBeamTypes = 0x65BC30;        // HolocaustBeam_Types: by +1 (one entry used)
constexpr std::uint32_t kBeamPhases = 0x65BC34;       // HolocaustBeam_Phases: four entries by +2
constexpr std::uint32_t kSparkTypes = 0x65BC44;       // HolocaustSpark_Types: by +1 (one entry used)
constexpr std::uint32_t kSparkPhases = 0x65BC48;      // HolocaustSpark_Phases: three entries by +2
constexpr std::uint32_t kMotes = 0x68E1B8;            // RottenBreath_Motes: 48 records of 0x84
constexpr std::uint32_t kStreaks = 0x698EC0;          // UtmostAttack_Streaks: 128 records of 0x3C
constexpr std::uint32_t kSparks = 0x6A2D38;           // Holocaust_Sparks: 64 records of 0x84
constexpr std::uint32_t kRecord = 0x84;
constexpr std::uint32_t kStreak = 0x3C;
// Capcom's, unnamed, outside the band and in no group: the map camera's
// rotation and translation set from Camera_Angles and the map focus (no
// arguments), and a world point (x, z, height << 16) projected into a
// primitive's vertex (docs/magic_c2.md section 5).
constexpr std::uint32_t kSetMapCamera = bof3::addr::EffectGte_LoadMapCamera;
constexpr std::uint32_t kProjectPoint = bof3::addr::EffectGte_ProjectPoint;
// Other units' functions (docs/magic_c2.md section 6) are ours now and called
// by name: the engine's MagicFx_WaitOwnerAnim, S38's MagicFx_CountDownFlag10,
// S37's MagicFx_FreeCurrentRecord.

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(at::kOwner); }
// [[0x93B8C4] + 0x80]: the current slot's owner - the caster's sprite.
unsigned char* Caster() {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Pointer(at::kCurrentSlot) + 0x80))));
}
unsigned char* TaskSlot(unsigned slot) { return Mem(at::kTasks + slot * at::kTaskStride); }
std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

std::uint16_t SW(unsigned k) { return Word(Mem(kS + k)); }
int SS(unsigned k) { return static_cast<short>(SW(k)); }
void SetSW(unsigned k, unsigned v) { SetWord(Mem(kS + k), v & 0xFFFF); }
void SetVW(unsigned k, unsigned v) { SetWord(Mem(kV + k), v & 0xFFFF); }
int VS(unsigned k) { return static_cast<short>(Word(Mem(kV + k))); }

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

int RandCall() { return MH_CALL(Rand)(); }
unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter); }
int Sin(int a) { return MH_CALL(Math_Sin)(a); }
int Cos(int a) { return MH_CALL(Math_Cos)(a); }
void CopyPosition(unsigned char* to, const unsigned char* from) {
    SetLong(to + 0x34, Long(from + 0x34));
    SetLong(to + 0x38, Long(from + 0x38));
    SetLong(to + 0x3C, Long(from + 0x3C));
}

// This group's functions called by address, as the originals call them: in the
// game the jmp Inject put there (or Capcom's code under BOF3X_ORIGINAL), in
// the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
using FnB = unsigned char (__cdecl*)();
using FnP = void (__cdecl*)(unsigned char*);
void Call0(std::uint32_t address) { MH_AT(Fn0, address)(); }

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
    const std::int32_t owner = Long(Mem(at::kOwner));
    for (unsigned i = 0; i < count; ++i) {
        unsigned char* const r = Mem(pool + i * kRecord);
        if (!(r[0] & 1)) continue;
        const std::int32_t its = Long(r + 0x80);
        Sprite_Current = r;
        SetLong(Mem(at::kOwner), its);
        Call0(fn);
        SetLong(Mem(at::kOwner), owner);
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

// ===========================================================================
// MAGIC057 (row 84, Bone Dance read one id down)

// original 0x4AE7C0: the kind-2 task. A three-entry stack table by +1:
// BoneDance_Start, BoneDance_Run, BoneDance_End.
C2_EXPORT void __cdecl BoneDance_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {addr::BoneDance_Start, addr::BoneDance_Run, addr::BoneDance_End};
    Step(kPhases, 3, Sc()[1], "BoneDance_Task");
}

// original 0x4AE7F0: the task at the owner's position; Gfx_ClutStripCopyRow(26),
// Gfx_ClutStripDirty up by one (read after the call); +1 on.
C2_EXPORT void __cdecl BoneDance_Start(void) {
    CopyPosition(Sc(), Owner());
    MH_CALL(Gfx_ClutStripCopyRow)(0x1A);
    Gfx_ClutStripDirty = static_cast<unsigned char>(Gfx_ClutStripDirty + 1);
    Inc(Sc()[1]);
}

// original 0x4AE850: a four-entry stack table by +2: BoneDance_Cast,
// BoneDance_Hold, BoneDance_Spawn, BoneDance_WaitBones.
C2_EXPORT void __cdecl BoneDance_Run(void) {
    static constexpr std::uint32_t kPhases[4] = {addr::BoneDance_Cast, addr::BoneDance_Hold, addr::BoneDance_Spawn,
                                                 addr::BoneDance_WaitBones};
    Step(kPhases, 4, Sc()[2], "BoneDance_Run");
}

// original 0x4AE890: sounds 0x101 and 0x102; the caster's animation 3; the
// shaker (kind 1, parameter 0x41) with +1 = 1, +2 = 0 and this task its owner,
// the slot unchecked; +2 on.
C2_EXPORT void __cdecl BoneDance_Cast(void) {
    MH_CALL(Sound_PlayById)(0x101);
    MH_CALL(Sound_PlayById)(0x102);
    CasterAnimation(3);
    const unsigned slot = NewTask(0x41) & 0xFF;
    unsigned char* const self = Sc();
    unsigned char* const t = TaskSlot(slot);
    t[1] = 1;
    t[2] = 0;
    SetLong(t + 0x80, static_cast<std::int32_t>(Key(self)));
    Inc(self[2]);
}

// original 0x4AE920: the caster's script ticked once (the answer unread); +9
// and +0xA 0. It never moves +2: the shaker does, after its third beat.
C2_EXPORT void __cdecl BoneDance_Hold(void) {
    unsigned char* const caster = Caster();
    unsigned char* const self = Sc();
    Sprite_Current = caster;
    MH_CALL(Sprite_ScriptTickOnce)();
    Sprite_Current = self;
    self[9] = 0;
    Sc()[0xA] = 0;
}

// original 0x4AE960: on odd frames, a bone (kind 1, parameter 0x41, +1 = 0) at
// the owner's position with this task its owner, counted in +9 - unless the
// pool is full (0xFF); at 0x20 bones +2 on instead.
C2_EXPORT void __cdecl BoneDance_Spawn(void) {
    if (!(Frame_Counter & 1)) return;
    unsigned char* const sc = Sc();
    if (sc[9] >= 0x20) {
        Inc(sc[2]);
        return;
    }
    const unsigned slot = NewTask(0x41) & 0xFF;
    if (slot == 0xFF) return;
    unsigned char* const t = TaskSlot(slot);
    t[1] = 0;
    t[2] = 0;
    const unsigned char* const owner = Owner();
    SetLong(t + 0x34, Long(owner + 0x34));
    SetLong(t + 0x38, Long(owner + 0x38));
    SetLong(t + 0x3C, Long(owner + 0x3C));
    unsigned char* const self = Sc();
    SetLong(t + 0x80, static_cast<std::int32_t>(Key(self)));
    Inc(self[9]);
}

// original 0x4AE9F0: every bone ended (+0xA, counted by BoneDanceBone_End, is
// +9): +1 on, +2 0.
C2_EXPORT void __cdecl BoneDance_WaitBones(void) {
    unsigned char* const sc = Sc();
    if (sc[9] != sc[0xA]) return;
    Inc(sc[1]);
    Sc()[2] = 0;
}

// original 0x4AEA20: the done flag, Battle_SetTargetFlag40(target), free.
C2_EXPORT void __cdecl BoneDance_End(void) {
    const unsigned target = Mem(at::kTarget)[0];
    Mem(at::kFlags)[0] = static_cast<unsigned char>(Mem(at::kFlags)[0] | 4);
    MH_CALL(Battle_SetTargetFlag40)(target);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4AEA40: the child (kind 1, parameter 0x41). A three-entry stack
// table by +1: a bone, the shaker, the follower.
C2_EXPORT void __cdecl BoneDanceChild_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {addr::BoneDanceBone_Run, addr::BoneDanceShake_Run,
                                                 addr::BoneDanceFollow_Run};
    Step(kPhases, 3, Sc()[1], "BoneDanceChild_Task");
}

// original 0x4AEA70: the frame-offset table 0x9039D8 at 0x8E3580; a
// three-entry stack table by +2 (start, fall, end); the sprite drawn while +0
// has bit 0; the table put back at 0x8B3580.
C2_EXPORT void __cdecl BoneDanceBone_Run(void) {
    static constexpr std::uint32_t kPhases[3] = {addr::BoneDanceBone_Start, addr::BoneDanceBone_Fall,
                                                 addr::BoneDanceBone_End};
    SetLong(Mem(kFrameSet), 0x8E3580);
    Step(kPhases, 3, Sc()[2], "BoneDanceBone_Run");
    if (Sc()[0] & 1) MH_CALL(Sprite_UpdateScreen)();
    SetLong(Mem(kFrameSet), 0x8B3580);
}

// original 0x4AEAD0: the bone placed round the owner - x by Rand % 6 + 2 (bit 0
// of +8) or Rand % 6 - 3, z by Rand % 6 + 3 or Rand % 6 + 2, in whole units
// (<< 16) - 0x600 higher, falling (+0x14 -32, +0x20 -1); its sprite fields; its
// animation 0; +9 3 (bounces); +2 on.
C2_EXPORT void __cdecl BoneDanceBone_Start(void) {
    int r = (Sc()[8] & 1) ? RandCall() % 6 + 2 : RandCall() % 6 - 3;
    SetLong(Sc() + 0x34, Add(static_cast<std::int32_t>(static_cast<std::uint32_t>(r) << 16), Long(Owner() + 0x34)));
    r = (Sc()[8] & 1) ? RandCall() % 6 + 3 : RandCall() % 6 + 2;
    SetLong(Sc() + 0x38, Add(static_cast<std::int32_t>(static_cast<std::uint32_t>(r) << 16), Long(Owner() + 0x38)));
    unsigned char* const s = Sc();
    SetWord(s + 0x3E, Word(s + 0x3E) + 0x600u);
    SetLong(s + 0x14, -32);
    SetLong(s + 0x20, -1);
    s[0x25] = 0x1D;
    s[0x26] = 0;
    s[0x48] = 1;
    s[0x27] = 0xA0;
    s[0x28] = 0;
    s[0x24] = 4;
    s[0x5D] = 0;
    s[0x5E] = 0;
    s[0x5F] = 0;
    s[0x5C] = 0;
    s[0x2A] = 0;
    s[0x29] = 4;
    SetWord(s + 0x2C, 0);
    MH_CALL(Sprite_SetAnimation)(0);
    Sc()[9] = 3;
    Inc(Sc()[2]);
}

// original 0x4AEC20: the height (+0x3E) down by +0x14's low word, +0x14 by
// +0x20; the sprite's script ticked; under the map's elevation at (x, z): a
// bounce - placed again round the owner (x by Rand % 6 + 2, z by Rand % 6 - 3,
// whatever +8), 0x600 higher, +0x14 -32 - and +9 down, at 0 +2 on.
C2_EXPORT void __cdecl BoneDanceBone_Fall(void) {
    unsigned char* s = Sc();
    SetWord(s + 0x3E, Word(s + 0x3E) + Word(s + 0x14));
    s = Sc();
    AddLong(s + 0x14, Long(s + 0x20));
    MH_CALL(Sprite_ScriptTick)();
    s = Sc();
    const long ground = MH_CALL(AreaMap_Elevation)(Long(s + 0x34), Long(s + 0x38));
    if (static_cast<short>(Word(Sc() + 0x3E)) >= static_cast<short>(ground)) return;
    int r = RandCall() % 6;
    SetLong(Sc() + 0x34, Add(static_cast<std::int32_t>(static_cast<std::uint32_t>(r + 2) << 16), Long(Owner() + 0x34)));
    r = RandCall() % 6;
    SetLong(Sc() + 0x38, Add(static_cast<std::int32_t>(static_cast<std::uint32_t>(r - 3) << 16), Long(Owner() + 0x38)));
    s = Sc();
    SetWord(s + 0x3E, Word(s + 0x3E) + 0x600u);
    SetLong(s + 0x14, -32);
    Dec(s[9]);
    if (s[9] == 0) Inc(s[2]);
}

// original 0x4AECF0: the owner's count of ended bones (+0xA) up; free.
C2_EXPORT void __cdecl BoneDanceBone_End(void) {
    Inc(Owner()[0xA]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4AED00: a three-entry stack table by +2: BoneDanceShake_Start,
// BoneDanceShake_Step, BattleFx_FreeTask.
C2_EXPORT void __cdecl BoneDanceShake_Run(void) {
    static constexpr std::uint32_t kPhases[3] = {addr::BoneDanceShake_Start, addr::BoneDanceShake_Step,
                                                 addr::BattleFx_FreeTask};
    Step(kPhases, 3, Sc()[2], "BoneDanceShake_Run");
}

// original 0x4AED30: beat +9 = 0, its length +0xA from BoneDanceShake_Counts
// (read in place by +9); sound 0x100; the offset +0x10 0; +2 on.
C2_EXPORT void __cdecl BoneDanceShake_Start(void) {
    Sc()[9] = 0;
    unsigned char* const s = Sc();
    s[0xA] = Mem(kShakeCounts)[s[9]];
    MH_CALL(Sound_PlayById)(0x100);
    SetLong(Sc() + 0x10, 0);
    Inc(Sc()[2]);
}

// original 0x4AED70: a two-entry stack table by +3 (down, up: the offset +0x10
// swings 0..-3); then, unless +0xA is 0xFF, the beat's length down - at 0 the
// next beat (+9 up, its length from the table, sound 0x100; at the third the
// owner's +2 on and +0xA 0xFF, else +3 0); when the owner has lost bit 0 of +0,
// the offset 0, +2 on, +3 0; every frame MapView_Redraw 2 and Camera_ShiftY
// the offset x 2.
C2_EXPORT void __cdecl BoneDanceShake_Step(void) {
    static constexpr std::uint32_t kPhases[2] = {addr::BoneDanceShake_Down, addr::BoneDanceShake_Up};
    Step(kPhases, 2, Sc()[3], "BoneDanceShake_Step");
    unsigned char* s = Sc();
    if (s[0xA] != 0xFF) {
        Dec(s[0xA]);
        if (Sc()[0xA] == 0) {
            s = Sc();
            Inc(s[9]);
            s = Sc();
            s[0xA] = Mem(kShakeCounts)[s[9]];
            MH_CALL(Sound_PlayById)(0x100);
            s = Sc();
            if (s[9] == 3) {
                Inc(Owner()[2]);
                Sc()[0xA] = 0xFF;
            } else {
                s[3] = 0;
            }
        }
    }
    if (!(Owner()[0] & 1)) {
        SetLong(Sc() + 0x10, 0);
        Inc(Sc()[2]);
        Sc()[3] = 0;
    }
    const std::uint16_t offset = Word(Sc() + 0x10);
    MapView_Redraw = 2;
    Camera_ShiftY = static_cast<short>(static_cast<std::uint16_t>(offset << 1));
}

// original 0x4AEE50: the offset +0x10 down while above -3; then +3 on.
C2_EXPORT void __cdecl BoneDanceShake_Down(void) {
    unsigned char* const s = Sc();
    const std::int32_t v = Long(s + 0x10);
    if (v > -3)
        SetLong(s + 0x10, Add(v, -1));
    else
        Inc(s[3]);
}

// original 0x4AEE70: the offset +0x10 up while below 0; then +3 back.
C2_EXPORT void __cdecl BoneDanceShake_Up(void) {
    unsigned char* const s = Sc();
    const std::int32_t v = Long(s + 0x10);
    if (v < 0)
        SetLong(s + 0x10, Add(v, 1));
    else
        Dec(s[3]);
}

// original 0x4AEEA0: the follower (+1 = 2; no creator in MAGIC057 sets it): the
// frame-offset table at 0x8E3580; a three-entry stack table by +2 (start,
// step, BattleFx_FreeTask); the sprite drawn while +0 has bit 0 and the
// owner's +0xA is 0; the table back at 0x8B3580.
C2_EXPORT void __cdecl BoneDanceFollow_Run(void) {
    static constexpr std::uint32_t kPhases[3] = {addr::BoneDanceFollow_Start, addr::BoneDanceFollow_Step,
                                                 addr::BattleFx_FreeTask};
    SetLong(Mem(kFrameSet), 0x8E3580);
    Step(kPhases, 3, Sc()[2], "BoneDanceFollow_Run");
    if ((Sc()[0] & 1) && Owner()[0xA] == 0) MH_CALL(Sprite_UpdateScreen)();
    SetLong(Mem(kFrameSet), 0x8B3580);
}

namespace {
// The follower's position: the owner's x and z less one, the height the map's
// elevation at (x, x) - the original passes x twice (docs/magic_c2.md
// section 8).
void FollowOwner() {
    SetLong(Sc() + 0x34, Add(Long(Owner() + 0x34), -1));
    SetLong(Sc() + 0x38, Add(Long(Owner() + 0x38), -1));
    const std::int32_t x = Long(Sc() + 0x34);
    const long ground = MH_CALL(AreaMap_Elevation)(x, x);
    SetWord(Sc() + 0x3E, static_cast<unsigned>(ground));
}
}  // namespace

// original 0x4AEF00: at the owner (FollowOwner); the sprite fields as the
// bone's but grey (+0x5D..+0x5F 0x80); animation 0; +2 on.
C2_EXPORT void __cdecl BoneDanceFollow_Start(void) {
    FollowOwner();
    unsigned char* const s = Sc();
    s[0x25] = 0x1D;
    s[0x26] = 0;
    s[0x48] = 1;
    s[0x27] = 0xA0;
    s[0x28] = 0;
    s[0x24] = 4;
    s[0x5D] = 0x80;
    s[0x5E] = 0x80;
    s[0x5F] = 0x80;
    s[0x5C] = 0;
    s[0x2A] = 0;
    s[0x29] = 4;
    SetWord(s + 0x2C, 0);
    MH_CALL(Sprite_SetAnimation)(0);
    Inc(Sc()[2]);
}

// original 0x4AEFE0: at the owner (FollowOwner); +2 on when the owner has lost
// bit 0 of +0.
C2_EXPORT void __cdecl BoneDanceFollow_Step(void) {
    FollowOwner();
    if (!(Owner()[0] & 1)) Inc(Sc()[2]);
}

// ===========================================================================
// MAGIC081 (row 85, RottenBreath read one id down)

// original 0x4BF8D0: the kind-2 task. A four-entry stack table by +1 -
// RottenBreath_Start, the engine's 0x43F430 (the caster's script to its end),
// RottenBreath_Emit, RottenBreath_End - then the walk of the 48 motes.
C2_EXPORT void __cdecl RottenBreath_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {addr::RottenBreath_Start, addr::MagicFx_WaitOwnerAnim,
                                                 addr::RottenBreath_Emit, addr::RottenBreath_End};
    Step(kPhases, 4, Sc()[1], "RottenBreath_Task");
    Walk(kMotes, 48, addr::RottenBreathMote_Run);
}

// original 0x4BF960: the motes' +0..+2 cleared; the task at the owner's x, z
// and height word; Gfx_ClutStripDirty 1 and row 26's first sixteen entries from
// their source with bit 15 set, then entry 0 again without it; the caster's
// animation 1; +1 on.
C2_EXPORT void __cdecl RottenBreath_Start(void) {
    for (unsigned i = 0; i < 48; ++i) {
        unsigned char* const r = Mem(kMotes + i * kRecord);
        r[0] = 0;
        r[1] = 0;
        r[2] = 0;
    }
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetWord(Sc() + 0x3E, Word(Owner() + 0x3E));
    Gfx_ClutStripDirty = 1;
    for (unsigned i = 0; i < 16; ++i) SetWord(Mem(kClutRow26 + 2 * i), Word(Mem(kClutRow26Source + 2 * i)) | 0x8000u);
    SetWord(Mem(kClutRow26), Word(Mem(kClutRow26Source)));
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
    SetLong(t + 0x80, static_cast<std::int32_t>(Key(self)));
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
    MH_CALL(Battle_SetTargetFlag40)(Mem(at::kTarget)[0]);
    Mem(at::kFlags)[0] = static_cast<unsigned char>(Mem(at::kFlags)[0] | 4);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4BFAE0: the child (kind 1, parameter 0x42): a jmp through
// RottenBreathChild_Types by +1, read in place, unchecked - the cloud (0) or
// the aim (1).
C2_EXPORT void __cdecl RottenBreathChild_Task(void) { CallCell(kRottenTypes, Sc()[1]); }

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
    SetLong(Sc() + 0x34, Add(Long(Owner() + 0x34), 0x20000));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetWord(Sc() + 0x3E, Word(Owner() + 0x3E));
    const unsigned slot = NewTask(0x42) & 0xFF;
    unsigned char* const self = Sc();
    unsigned char* const t = TaskSlot(slot);
    SetLong(t + 0x80, static_cast<std::int32_t>(Key(self)));
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
        unsigned char* const r = Mem(kMotes + m * kRecord);
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
    if (Mem(at::kFlags)[0] & 4)
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
    SetLong(Sc() + 0x34, Add(Long(Owner() + 0x34), 0x2000));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetWord(Sc() + 0x3E, Word(Owner() + 0x3E) + 0x100u);
    const unsigned char* const aim = TaskSlot(Owner()[0xA]);
    const unsigned char* const s = Sc();
    const std::int32_t dz = Add(Long(aim + 0x38), -Long(s + 0x38));
    const std::int32_t dx = Add(Long(aim + 0x34), -Long(s + 0x34));
    const int heading = MH_CALL(Math_Ratan2)(static_cast<float>(dx), static_cast<float>(dz));
    SetLong(Sc() + 0x14, heading);
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
    SetLong(Sc() + 0xC, Add(static_cast<std::int32_t>(static_cast<std::uint32_t>(v) * 300u) >> 12, Long(s + 0x14)));
    unsigned char* const px = Sc() + 0x34;
    v = Sin(Long(Sc() + 0xC));
    AddLong(px, Shl12(static_cast<int>(static_cast<std::uint32_t>(v) * 7u), 12));
    unsigned char* const pz = Sc() + 0x38;
    v = Cos(Long(Sc() + 0xC));
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
        MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(s + 0x34)), static_cast<unsigned long>(Long(s + 0x38)), 2,
                                    0xC);
    }
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyGT4)(p);
    {
        const unsigned char* const s = Sc();
        const unsigned n = s[9];
        SetSW(0, n < 8 ? n * 8 + 8 : 0x40);
        SetSW(8, 0x80);
        SetSW(4, Word(s + 0x2E));
        SetSW(6, Word(s + 0x30));
    }
    static constexpr int kAngles[4] = {0xC00, 0x800, 0, 0x400};
    static constexpr unsigned kAt[4] = {8, 0x1C, 0x30, 0x44};
    for (unsigned c = 0; c < 4; ++c) {
        int v = Sin(kAngles[c]);
        PutFloat(p + kAt[c], Add(Mul12(v, SS(0)), SS(4)));
        v = Cos(kAngles[c]);
        PutFloat(p + kAt[c] + 4, Add(Mul12(v, SS(0)), SS(6)));
    }
    SetWord(p + 0x2A, MH_CALL(Gpu_GetTPage)(0, 2, 0x340, 0x100));
    SetWord(p + 0x16, MH_CALL(Gpu_GetClut)(0, 0x1FA));
    p[0x14] = 1;
    p[0x15] = 1;
    p[0x28] = 0x1F;
    p[0x29] = 1;
    p[0x3C] = 1;
    p[0x3D] = 0x1F;
    p[0x50] = 0x1F;
    p[0x51] = 0x1F;
    static constexpr unsigned kShade[12] = {4, 5, 6, 0x18, 0x19, 0x1A, 0x2C, 0x2D, 0x2E, 0x40, 0x41, 0x42};
    for (const unsigned k : kShade) p[k] = Mem(kS + 8)[0];
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    const unsigned char* const s = Sc();
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(s + 0x34)), static_cast<unsigned long>(Long(s + 0x38)), 2,
                                0x54);
}

// original 0x4C0190: the first of the 48 motes without bit 0 of +0 gets it; its
// index in al, 0xFF when all are taken.
C2_EXPORT unsigned char __cdecl RottenBreathMote_Alloc(void) { return Alloc(kMotes, 48); }

// ===========================================================================
// MAGIC116 (row 59, UtmostAttack read one id down)

// original 0x4D9AE0: the kind-2 task. A four-entry stack table by +1:
// UtmostAttack_Start, _WaitCaster, _Stream, _End.
C2_EXPORT void __cdecl UtmostAttack_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {addr::UtmostAttack_Start, addr::UtmostAttack_WaitCaster,
                                                 addr::UtmostAttack_Stream, addr::UtmostAttack_End};
    Step(kPhases, 4, Sc()[1], "UtmostAttack_Task");
}

// original 0x4D9B20: the task at the caster, 0x100 units higher (+0x3C +
// 0x1000000); the streaks cleared; the caster's animation 1; +1 on.
C2_EXPORT void __cdecl UtmostAttack_Start(void) {
    SetLong(Sc() + 0x34, Long(Caster() + 0x34));
    SetLong(Sc() + 0x38, Long(Caster() + 0x38));
    SetLong(Sc() + 0x3C, Add(Long(Caster() + 0x3C), 0x1000000));
    Call0(addr::UtmostAttack_ClearStreaks);
    CasterAnimation(1);
    Inc(Sc()[1]);
}

// original 0x4D9BB0: the caster's script ticked once; when it reports its end,
// +9 0x78 (frames of streaks), +1 on, sound effect 0x100 (Sprite_Current put
// back after it too).
C2_EXPORT void __cdecl UtmostAttack_WaitCaster(void) {
    unsigned char* const caster = Caster();
    unsigned char* const self = Sc();
    Sprite_Current = caster;
    const unsigned char done = MH_CALL(Sprite_ScriptTickOnce)();
    Sprite_Current = self;
    if (!done) return;
    self[9] = 0x78;
    Inc(Sc()[1]);
    MH_CALL(Sound_PlayEffect)(0x100);
    Sprite_Current = self;
}

// original 0x4D9C00: eight streaks spawned, every streak drawn and moved (the
// answer unread); +9 down, at 0 +1 on, Battle_SetTargetFlag40(target), sound
// effect 0x101.
C2_EXPORT void __cdecl UtmostAttack_Stream(void) {
    Call0(addr::UtmostAttack_SpawnStreaks);
    MH_AT(FnB, addr::UtmostAttack_DrawStreaks)();
    Dec(Sc()[9]);
    unsigned char* const s = Sc();
    if (s[9] != 0) return;
    Inc(s[1]);
    MH_CALL(Battle_SetTargetFlag40)(Mem(at::kTarget)[0]);
    MH_CALL(Sound_PlayEffect)(0x101);
}

// original 0x4D9C50: the streaks drawn and moved until none is left; then the
// done flag and free (a tail jmp).
C2_EXPORT void __cdecl UtmostAttack_End(void) {
    if (MH_AT(FnB, addr::UtmostAttack_DrawStreaks)() != 0) return;
    Mem(at::kFlags)[0] = static_cast<unsigned char>(Mem(at::kFlags)[0] | 4);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4D9C70: +0 of the 128 streaks cleared.
C2_EXPORT void __cdecl UtmostAttack_ClearStreaks(void) {
    for (unsigned i = 0; i < 128; ++i) Mem(kStreaks + i * kStreak)[0] = 0;
}

// original 0x4D9C90: a draw-mode packet (tpage Gpu_GetTPage(0, 1, 0x2C0, 0x100),
// committed at 3); the map camera (0x494060); every live streak drawn, then
// both its points moved by its velocity (+0x24, +0x28, +0x2C, read after the
// draw) and its life +2 down, +0 cleared at 0. Answers in al whether any
// streak was live.
C2_EXPORT unsigned char __cdecl UtmostAttack_DrawStreaks(void) {
    unsigned char any = 0;
    const unsigned tpage = MH_CALL(Gpu_GetTPage)(0, 1, 0x2C0, 0x100) & 0xFFFF;
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, tpage, 0);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
    Call0(kSetMapCamera);
    for (unsigned i = 0; i < 128; ++i) {
        unsigned char* const r = Mem(kStreaks + i * kStreak);
        if (r[0] == 0) continue;
        MH_AT(FnP, addr::UtmostAttack_DrawStreak)(r);
        const std::int32_t vx = Long(r + 0x24), vz = Long(r + 0x28), vy = Long(r + 0x2C);
        AddLong(r + 4, vx);
        AddLong(r + 8, vz);
        AddLong(r + 0xC, vy);
        AddLong(r + 0x14, vx);
        AddLong(r + 0x18, vz);
        AddLong(r + 0x1C, vy);
        Dec(r[2]);
        any = 1;
        if (r[2] == 0) r[0] = 0;
    }
    return any;
}

// original 0x4D9D50: one semi-transparent gouraud quad (Gpu_SetPolyG4): the
// streak's first point (+4, +8, +0xC) 0x1000 either side in x and its second
// (+0x14, +0x18, +0x1C) likewise, each projected by 0x494110 from a vector in
// the caller's frame (x changed between the two of a point, z and height
// kept); the first two vertices +0x34..+0x36's colour, the last two
// +0x38..+0x3A's; committed at 3, 0x44 bytes.
C2_EXPORT void __cdecl UtmostAttack_DrawStreak(unsigned char* streak) {
    using ProjectFn = void (__cdecl*)(const std::int32_t*, unsigned char*);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyG4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    std::int32_t v[3] = {Add(Long(streak + 4), 0x1000), Long(streak + 8), Long(streak + 0xC)};
    MH_AT(ProjectFn, kProjectPoint)(v, p + 8);
    v[0] = Add(Long(streak + 4), -0x1000);
    MH_AT(ProjectFn, kProjectPoint)(v, p + 0x18);
    v[0] = Add(Long(streak + 0x14), 0x1000);
    v[1] = Long(streak + 0x18);
    v[2] = Long(streak + 0x1C);
    MH_AT(ProjectFn, kProjectPoint)(v, p + 0x28);
    v[0] = Add(Long(streak + 0x14), -0x1000);
    MH_AT(ProjectFn, kProjectPoint)(v, p + 0x38);
    for (unsigned k = 0; k < 3; ++k) {
        const unsigned char first = streak[0x34 + k];
        p[0x14 + k] = first;
        p[4 + k] = first;
        const unsigned char second = streak[0x38 + k];
        p[0x34 + k] = second;
        p[0x24 + k] = second;
    }
    MH_CALL(Gfx_CommitPrim)(3, 0x44);
}

// original 0x4D9E40: eight times, a free streak (UtmostAttack_FreeStreak)
// started (UtmostAttack_InitStreak) if there is one.
C2_EXPORT void __cdecl UtmostAttack_SpawnStreaks(void) {
    using FindFn = unsigned char* (__cdecl*)();
    for (unsigned n = 0; n < 8; ++n) {
        unsigned char* const r = MH_AT(FindFn, addr::UtmostAttack_FreeStreak)();
        if (r) MH_AT(FnP, addr::UtmostAttack_InitStreak)(r);
    }
}

// original 0x4D9E60: the first streak whose +0 is 0, or null.
C2_EXPORT unsigned char* __cdecl UtmostAttack_FreeStreak(void) {
    for (unsigned i = 0; i < 128; ++i) {
        unsigned char* const r = Mem(kStreaks + i * kStreak);
        if (r[0] == 0) return r;
    }
    return nullptr;
}

// original 0x4D9E80: +0 1, +1 0, life +2 0x10; d = ((Rand & 0x1FF) - 0x100) <<
// 8; the first point the task's x + 2d, z + 2 units, height; the second the
// task's x + d, z, height; colours +0x34..+0x36 each Rand << 7 (0 or 0x80),
// +0x38..+0x3A 0; the velocity (d / 2, one unit, 0).
C2_EXPORT void __cdecl UtmostAttack_InitStreak(unsigned char* streak) {
    streak[0] = 1;
    streak[1] = 0;
    streak[2] = 0x10;
    const std::int32_t d =
        static_cast<std::int32_t>(((static_cast<std::uint32_t>(RandCall()) & 0x1FF) - 0x100) << 8);
    SetLong(streak + 4, Add(Long(Sc() + 0x34), static_cast<std::int32_t>(static_cast<std::uint32_t>(d) * 2)));
    SetLong(streak + 8, Add(Long(Sc() + 0x38), 0x20000));
    SetLong(streak + 0xC, Long(Sc() + 0x3C));
    SetLong(streak + 0x14, Add(Long(Sc() + 0x34), d));
    SetLong(streak + 0x18, Long(Sc() + 0x38));
    SetLong(streak + 0x1C, Long(Sc() + 0x3C));
    streak[0x34] = static_cast<unsigned char>(static_cast<unsigned>(RandCall()) << 7);
    streak[0x35] = static_cast<unsigned char>(static_cast<unsigned>(RandCall()) << 7);
    const auto blue = static_cast<unsigned char>(static_cast<unsigned>(RandCall()) << 7);
    SetLong(streak + 0x24, d >> 1);
    streak[0x36] = blue;
    SetLong(streak + 0x28, 0x10000);
    SetLong(streak + 0x2C, 0);
    streak[0x38] = 0;
    streak[0x39] = 0;
    streak[0x3A] = 0;
}

// ===========================================================================
// MAGIC129 (row 145, Holocaust read one id down)

// original 0x4E3260: the kind-2 task. A three-entry stack table by +1 -
// Holocaust_Start, MAGIC226/227's 0x4F9F70 (+9 down, then target flag 0x10),
// BattleFx_Finish - then the walk of the 64 sparks.
C2_EXPORT void __cdecl Holocaust_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {addr::Holocaust_Start, addr::MagicFx_CountDownFlag10,
                                                 addr::BattleFx_Finish};
    Step(kPhases, 3, Sc()[1], "Holocaust_Task");
    Walk(kSparks, 64, addr::HolocaustSpark_Task);
}

// original 0x4E32E0: the sparks' +0..+2 cleared; the task at the target side's
// centre; +0xB 0, +9 0x38, +1 on. With the target's side bit 0x40 (the
// enemies) +1 2 and nothing more; else a beam (kind 1, parameter 0x6A) for each
// party member 0..2 with bit 0 of its record's +0: this task its owner, +1 0,
// +4 the member, +0xB the member << 4, +9 0x3C, counted in +0xB.
C2_EXPORT void __cdecl Holocaust_Start(void) {
    for (unsigned i = 0; i < 64; ++i) {
        unsigned char* const r = Mem(kSparks + i * kRecord);
        r[0] = 0;
        r[1] = 0;
        r[2] = 0;
    }
    MH_CALL(MagicFx_CenterOnSide)();
    Sc()[0xB] = 0;
    Sc()[9] = 0x38;
    Inc(Sc()[1]);
    if (Mem(at::kTarget)[0] & 0x40) {
        Sc()[1] = 2;
        return;
    }
    for (unsigned member = 0; member < 3; ++member) {
        if (!(Mem(at::kParty + member * at::kPartyStride)[0] & 1)) continue;
        const unsigned slot = NewTask(0x6A) & 0xFF;
        unsigned char* const self = Sc();
        unsigned char* const t = TaskSlot(slot);
        SetLong(t + 0x80, static_cast<std::int32_t>(Key(self)));
        t[1] = 0;
        t[4] = static_cast<unsigned char>(member);
        t[0xB] = static_cast<unsigned char>(member << 4);
        t[9] = 0x3C;
        Inc(self[0xB]);
    }
}

// original 0x4E33B0: the beam (kind 1, parameter 0x6A): a jmp through
// HolocaustBeam_Types by +1, read in place, unchecked.
C2_EXPORT void __cdecl HolocaustBeam_Task(void) { CallCell(kBeamTypes, Sc()[1]); }

// original 0x4E33D0: a call through HolocaustBeam_Phases by +2, read in place,
// unchecked; +0xB up (the band's wave); while +0 and +2 are set, the band
// (HolocaustBeam_Draw, a tail jmp).
C2_EXPORT void __cdecl HolocaustBeam_Run(void) {
    CallCell(kBeamPhases, Sc()[2]);
    Inc(Sc()[0xB]);
    const unsigned char* const s = Sc();
    if (s[0] != 0 && s[2] != 0) Call0(addr::HolocaustBeam_Draw);
}

namespace {
unsigned char* EnemyRecord(int i) {
    return Mem(static_cast<std::uint32_t>(static_cast<int>(at::kEnemies) + i * static_cast<int>(at::kEnemyStride)));
}
unsigned char* PartyRecord(unsigned i) { return Mem(at::kParty + i * at::kPartyStride); }
}  // namespace

// original 0x4E3410: +9 down; at 0 the beam aims. Its far end: the task at the
// member +4 names - enemy record +4 (no - 3) when the target byte has 0x40,
// else party record +4 - one unit on in x, its screen point to the scratch
// words 0x90385C / 0x90385E. Its near end: the acting actor's enemy record
// (actor - 3, unchecked: a party actor reads below the enemies), one unit back
// in x and 0x360 higher, its screen point to 0x903858 / 0x90385A. The step
// (+0xC, +0x10) a sixteenth of the difference, the heading +0x14 Math_Ratan2 of
// it & 0xFFF; +0x5D 0x10 (the fade), +9 0x96, +0xA 0 (the band's length), +2 on;
// sound 0x100.
C2_EXPORT void __cdecl HolocaustBeam_Aim(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    unsigned char* s = Sc();
    const unsigned char* const far_end = (Mem(at::kTarget)[0] & 0x40) ? EnemyRecord(s[4]) : PartyRecord(s[4]);
    CopyPosition(s, far_end);
    AddLong(Sc() + 0x34, 0x10000);
    MH_CALL(BattleActor_UpdateScreenXY)();
    s = Sc();
    SetSW(0xC, Word(s + 0x2E));
    SetSW(0xE, Word(s + 0x30));
    const unsigned char* const near_end = EnemyRecord(static_cast<int>(Mem(at::kActor)[0]) - 3);
    CopyPosition(s, near_end);
    AddLong(Sc() + 0x34, -0x10000);
    AddLong(Sc() + 0x3C, 0x3600000);
    MH_CALL(BattleActor_UpdateScreenXY)();
    s = Sc();
    SetSW(8, Word(s + 0x2E));
    SetSW(0xA, Word(s + 0x30));
    SetLong(s + 0xC, (SS(0xC) - SS(8)) / 16);
    SetLong(Sc() + 0x10, (SS(0xE) - SS(0xA)) / 16);
    const int dy = SS(0xE) - SS(0xA);
    const int dx = SS(0xC) - SS(8);
    const int heading = MH_CALL(Math_Ratan2)(static_cast<float>(dx), static_cast<float>(dy));
    SetLong(Sc() + 0x14, heading & 0xFFF);
    Sc()[0x5D] = 0x10;
    Sc()[9] = 0x96;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4E3660: the band's length +0xA up by two; at 0x10 +2 on.
C2_EXPORT void __cdecl HolocaustBeam_Grow(void) {
    unsigned char* const s = Sc();
    s[0xA] = static_cast<unsigned char>(s[0xA] + 2);
    if (Sc()[0xA] == 0x10) Inc(Sc()[2]);
}

// original 0x4E3680: while +9 is above 0x10, every fourth frame a spark
// (HolocaustSpark_Alloc, unchecked) owned by the beam's owner, +1 0, its angle
// +0xB Rand & 0x1F, its delay +9 1, counted in the owner's +0xB; +9 down, at 0
// +2 on.
C2_EXPORT void __cdecl HolocaustBeam_Emit(void) {
    const unsigned char* const s = Sc();
    if (s[9] > 0x10 && (Frame_Counter & 3) == 0) {
        const unsigned k = MH_AT(FnB, addr::HolocaustSpark_Alloc)() & 0xFFu;
        unsigned char* const r = Mem(kSparks + k * kRecord);
        SetLong(r + 0x80, Long(Mem(at::kOwner)));
        r[1] = 0;
        r[0xB] = static_cast<unsigned char>(RandCall() & 0x1F);
        unsigned char* const owner = Owner();
        r[9] = 1;
        Inc(owner[0xB]);
    }
    Dec(Sc()[9]);
    if (Sc()[9] == 0) Inc(Sc()[2]);
}

// original 0x4E3710: the fade +0x5D down; at 0 the owner's count down and free.
C2_EXPORT void __cdecl HolocaustBeam_Fade(void) {
    Dec(Sc()[0x5D]);
    if (Sc()[0x5D] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

namespace {
// The band's quads: the ends' colours set to 1 at the band's first segment
// (i 1) and its last (i +0xA).
void BandEnds(unsigned char* p, unsigned char i) {
    if (i == Sc()[0xA]) {
        for (unsigned k = 0; k < 3; ++k) {
            p[0x24 + k] = 1;
            p[0x34 + k] = 1;
        }
    }
    if (i == 1) {
        for (unsigned k = 0; k < 3; ++k) {
            p[4 + k] = 1;
            p[0x14 + k] = 1;
        }
    }
}
unsigned char* NewG4() {
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyG4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    return p;
}
// The angle pair's first: (the dword 0x903854 +- 0x400) & 0xFFF; its second:
// (the word 0x903856 +- 0x400) & 0xFFF.
int Across54(int delta) {
    return static_cast<int>((static_cast<std::uint32_t>(Long(Mem(kS + 4))) + static_cast<std::uint32_t>(delta)) & 0xFFF);
}
int Across56(int delta) { return static_cast<int>((SW(6) + static_cast<unsigned>(delta)) & 0xFFF); }
}  // namespace

// original 0x4E3740: the band. Tpage 0x35 committed at 2; the scratch words:
// 0x903850 / 0x903852 the previous and this segment's half-width, 0x903854 /
// 0x903856 their headings (the first (+0x14 - 0x800) & 0xFFF), 0x903858 /
// 0x90385A this segment's centre, 0x90385C / 0x90385E the previous one's (the
// first the beam's screen point, +0x2E / +0x30). For i 1..+0xA (read again each
// time): the half-width 8 + sin(i << 7) >> 7; the centre the screen point + i x
// the step (+0xC, +0x10, 16-bit), waved across the heading + 0x400 by
// sin((+0xB + i) & 0x3F << 6) x (sin(i << 7) x 48 >> 12) >> 12; the heading
// Math_Ratan2 from the previous centre; then four semi-transparent gouraud
// quads between the two segments (inner and outer edge, each side), colours
// +0x5D x 15 / x 12 / x 6 fading to 1 at the rims and at the band's two ends,
// committed at 2, 0x44 bytes each. Then tpage 0x15 committed at 2.
C2_EXPORT void __cdecl HolocaustBeam_Draw(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    MH_CALL(Gfx_CommitPrim)(2, 0xC);
    {
        const unsigned char* const s = Sc();
        const unsigned char fade = s[0x5D];
        SetSW(2, 8);
        const auto c15 = static_cast<unsigned char>(fade * 15), c12 = static_cast<unsigned char>(fade * 12),
                   c6 = static_cast<unsigned char>(fade * 6);
        SetSW(6, (Word(s + 0x14) - 0x800u) & 0xFFF);
        SetSW(8, Word(s + 0x2E));
        SetSW(0xA, Word(s + 0x30));
        if (static_cast<int>(s[0xA]) + 1 > 1) {
            unsigned char i = 1;
            do {
                SetSW(0, SW(2));
                int v = Sin(static_cast<int>(i) << 7);
                const std::uint16_t prev_x = SW(8), prev_y = SW(0xA);
                SetSW(2, static_cast<unsigned>(Shl12(v, 5) + 8));
                SetSW(0xC, prev_x);
                const unsigned char* t = Sc();
                SetSW(0xE, prev_y);
                SetSW(8, static_cast<unsigned>(Word(t + 0xC) * i + Word(t + 0x2E)));
                SetSW(0xA, static_cast<unsigned>(Word(t + 0x10) * i + Word(t + 0x30)));
                const int across = static_cast<int>((Word(t + 0x14) + 0x400u) & 0xFFF);
                v = Sin(static_cast<int>(i) << 7);
                const int swing = static_cast<std::int32_t>(static_cast<std::uint32_t>(v) * 48u) >> 12;
                t = Sc();
                v = Sin(static_cast<int>(((static_cast<unsigned>(t[0xB]) + i) & 0x3F) << 6));
                const int wave = static_cast<short>(Mul12(v, static_cast<short>(swing)));
                v = Sin(across);
                SetSW(8, SW(8) + static_cast<unsigned>(Mul12(v, wave)));
                v = Cos(across);
                const auto y = static_cast<std::uint16_t>(SW(0xA) + static_cast<unsigned>(Mul12(v, wave)));
                SetSW(4, SW(6));
                const int ex = SS(0xE) - static_cast<short>(y);
                SetSW(0xA, y);
                const int ey = SS(0xC) - SS(8);
                const int heading = MH_CALL(Math_Ratan2)(static_cast<float>(ey), static_cast<float>(ex));
                unsigned char* p = Gfx_PacketNext;
                SetSW(6, static_cast<unsigned>(heading) & 0xFFF);
                MH_CALL(Gpu_SetPolyG4)(p);
                MH_CALL(Gpu_SetSemiTrans)(p, 1);

                // 1: from each centre out by 8 across the heading + 0x400
                int a = Across54(0x400);
                v = Sin(a);
                PutFloat(p + 0x18, Shl12(v, 3) + SS(0xC));
                v = Cos(a);
                PutFloat(p + 0x1C, Shl12(v, 3) + SS(0xE));
                a = Across56(0x400);
                v = Sin(a);
                PutFloat(p + 0x38, Shl12(v, 3) + SS(8));
                v = Cos(a);
                PutFloat(p + 0x3C, Shl12(v, 3) + SS(0xA));
                PutFloat(p + 8, SS(0xC));
                PutFloat(p + 0xC, SS(0xE));
                PutFloat(p + 0x28, SS(8));
                PutFloat(p + 0x2C, SS(0xA));
                for (unsigned k = 0; k < 3; ++k) {
                    p[4 + k] = c15;
                    p[0x24 + k] = c15;
                }
                p[0x14] = c12;
                p[0x15] = c6;
                p[0x16] = c15;
                p[0x34] = c12;
                p[0x35] = c6;
                p[0x36] = c15;
                BandEnds(p, i);
                MH_CALL(Gfx_CommitPrim)(2, 0x44);

                // 2: from 8 out to the half-width, the heading + 0x400 side
                p = NewG4();
                a = Across54(0x400);
                v = Sin(a);
                PutFloat(p + 0x18, Mul12(v, SS(0)) + SS(0xC));
                v = Cos(a);
                PutFloat(p + 0x1C, Mul12(v, SS(0)) + SS(0xE));
                v = Sin(a);
                PutFloat(p + 8, Shl12(v, 3) + SS(0xC));
                v = Cos(a);
                PutFloat(p + 0xC, Shl12(v, 3) + SS(0xE));
                a = Across56(0x400);
                v = Sin(a);
                PutFloat(p + 0x38, Mul12(v, SS(2)) + SS(8));
                v = Cos(a);
                PutFloat(p + 0x3C, Mul12(v, SS(2)) + SS(0xA));
                v = Sin(a);
                PutFloat(p + 0x28, Shl12(v, 3) + SS(8));
                v = Cos(a);
                PutFloat(p + 0x2C, Shl12(v, 3) + SS(0xA));
                p[4] = c12;
                p[5] = c6;
                p[6] = c15;
                p[0x24] = c12;
                p[0x25] = c6;
                p[0x26] = c15;
                for (unsigned k = 0; k < 3; ++k) {
                    p[0x14 + k] = 1;
                    p[0x34 + k] = 1;
                }
                BandEnds(p, i);
                MH_CALL(Gfx_CommitPrim)(2, 0x44);

                // 3: the centre out by 8 across the heading - 0x400
                p = NewG4();
                a = Across54(-0x400);
                v = Sin(a);
                PutFloat(p + 8, Shl12(v, 3) + SS(0xC));
                v = Cos(a);
                PutFloat(p + 0xC, Shl12(v, 3) + SS(0xE));
                a = Across56(-0x400);
                v = Sin(a);
                PutFloat(p + 0x28, Shl12(v, 3) + SS(8));
                v = Cos(a);
                PutFloat(p + 0x2C, Shl12(v, 3) + SS(0xA));
                PutFloat(p + 0x18, SS(0xC));
                PutFloat(p + 0x1C, SS(0xE));
                PutFloat(p + 0x38, SS(8));
                PutFloat(p + 0x3C, SS(0xA));
                for (unsigned k = 0; k < 3; ++k) {
                    p[0x14 + k] = c15;
                    p[0x34 + k] = c15;
                }
                p[4] = c12;
                p[5] = c6;
                p[6] = c15;
                p[0x24] = c12;
                p[0x25] = c6;
                p[0x26] = c15;
                BandEnds(p, i);
                MH_CALL(Gfx_CommitPrim)(2, 0x44);

                // 4: from the half-width in to 8, the heading - 0x400 side
                p = NewG4();
                a = Across54(-0x400);
                v = Sin(a);
                PutFloat(p + 8, Mul12(v, SS(0)) + SS(0xC));
                v = Cos(a);
                PutFloat(p + 0xC, Mul12(v, SS(0)) + SS(0xE));
                v = Sin(a);
                PutFloat(p + 0x18, Shl12(v, 3) + SS(0xC));
                v = Cos(a);
                PutFloat(p + 0x1C, Shl12(v, 3) + SS(0xE));
                a = Across56(-0x400);
                v = Sin(a);
                PutFloat(p + 0x28, Mul12(v, SS(2)) + SS(8));
                v = Cos(a);
                PutFloat(p + 0x2C, Mul12(v, SS(2)) + SS(0xA));
                v = Sin(a);
                PutFloat(p + 0x38, Shl12(v, 3) + SS(8));
                v = Cos(a);
                PutFloat(p + 0x3C, Shl12(v, 3) + SS(0xA));
                p[0x14] = c12;
                p[0x15] = c6;
                p[0x16] = c15;
                p[0x34] = c12;
                p[0x35] = c6;
                p[0x36] = c15;
                for (unsigned k = 0; k < 3; ++k) {
                    p[4 + k] = 1;
                    p[0x24 + k] = 1;
                }
                BandEnds(p, i);
                MH_CALL(Gfx_CommitPrim)(2, 0x44);
                ++i;
            } while (static_cast<int>(i) < static_cast<int>(Sc()[0xA]) + 1);
        }
    }
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    MH_CALL(Gfx_CommitPrim)(2, 0xC);
}

// original 0x4E4000: a spark (a pool record): a jmp through HolocaustSpark_Types
// by +1, read in place, unchecked.
C2_EXPORT void __cdecl HolocaustSpark_Task(void) { CallCell(kSparkTypes, Sc()[1]); }

// original 0x4E4020: a call through HolocaustSpark_Phases by +2, read in place,
// unchecked; while +0 and +2 are set, tpage 0x35, the spark's screen point and
// its disc (HolocaustSpark_Draw), tpage 0x15 - each draw mode linked at the
// spark's x, z.
C2_EXPORT void __cdecl HolocaustSpark_Run(void) {
    CallCell(kSparkPhases, Sc()[2]);
    const unsigned char* s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    s = Sc();
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(s + 0x34)), static_cast<unsigned long>(Long(s + 0x38)), 2, 0xC);
    MH_CALL(BattleActor_UpdateScreenXY)();
    Call0(addr::HolocaustSpark_Draw);
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    s = Sc();
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(s + 0x34)), static_cast<unsigned long>(Long(s + 0x38)), 2, 0xC);
}

// original 0x4E40B0: +9 (the delay) down; at 0 the spark starts: rising (+0x14
// 0x40000, +0x20 0x20000), at the owner's position moved by its angle (+0xB &
// 0x1F) << 7 times a radius Rand & 0xF + 0x1C (sin and cos unshifted), the
// owner's height; colours +0x5D..+0x5F each Rand & 7 + 5; +9 and +0xA 0x10; +2 on.
C2_EXPORT void __cdecl HolocaustSpark_Start(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    SetLong(Sc() + 0x14, 0x40000);
    SetLong(Sc() + 0x20, 0x20000);
    const std::uint32_t roll = static_cast<std::uint32_t>(RandCall());
    const int radius = static_cast<short>((roll & 0xF) + 0x1C);
    const int angle = static_cast<short>((Sc()[0xB] & 0x1F) << 7);
    int v = Sin(angle);
    SetLong(Sc() + 0x34, Add(static_cast<std::int32_t>(static_cast<std::uint32_t>(v) * static_cast<std::uint32_t>(radius)),
                             Long(Owner() + 0x34)));
    v = Cos(angle);
    SetLong(Sc() + 0x38, Add(static_cast<std::int32_t>(static_cast<std::uint32_t>(v) * static_cast<std::uint32_t>(radius)),
                             Long(Owner() + 0x38)));
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
    Sc()[0x5D] = static_cast<unsigned char>((RandCall() & 7) + 5);
    Sc()[0x5E] = static_cast<unsigned char>((RandCall() & 7) + 5);
    Sc()[0x5F] = static_cast<unsigned char>((RandCall() & 7) + 5);
    Sc()[9] = 0x10;
    Sc()[0xA] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4E41B0: +0x14 up by +0x20, the height +0x3C by +0x14; +9 down, at 0
// +2 on.
C2_EXPORT void __cdecl HolocaustSpark_Rise(void) {
    unsigned char* s = Sc();
    AddLong(s + 0x14, Long(s + 0x20));
    s = Sc();
    AddLong(s + 0x3C, Long(s + 0x14));
    Dec(Sc()[9]);
    if (Sc()[9] == 0) Inc(Sc()[2]);
}

// original 0x4E41F0: rising as above; the fade +0xA down, at 0 the owner's
// count down and MAGIC219's 0x4F6290 (+0..+4 cleared: the record free), a tail
// jmp.
C2_EXPORT void __cdecl HolocaustSpark_Fade(void) {
    unsigned char* s = Sc();
    AddLong(s + 0x14, Long(s + 0x20));
    s = Sc();
    AddLong(s + 0x3C, Long(s + 0x14));
    Dec(Sc()[0xA]);
    if (Sc()[0xA] != 0) return;
    Dec(Owner()[0xB]);
    Call0(addr::MagicFx_FreeCurrentRecord);
}

// original 0x4E4240: a disc of eight semi-transparent gouraud triangles round
// the spark's screen point, radius 32 (sin << 5 >> 12), the rim point kept in
// Prim_VertexScratch's first two words from one triangle to the next; the
// centre colour +0x5D..+0x5F x the fade +0xA, the rim 1; each linked at the
// spark's x, z (0x34 bytes).
C2_EXPORT void __cdecl HolocaustSpark_Draw(void) {
    int v = Sin(0);
    SetVW(0, static_cast<unsigned>(Shl12(v, 5)) + Word(Sc() + 0x2E));
    v = Cos(0);
    const unsigned char* s = Sc();
    SetVW(2, static_cast<unsigned>(Shl12(v, 5)) + Word(s + 0x30));
    const unsigned char fade = s[0xA];
    const auto red = static_cast<unsigned char>(s[0x5D] * fade), green = static_cast<unsigned char>(s[0x5E] * fade),
               blue = static_cast<unsigned char>(s[0x5F] * fade);
    int angle = 0x200;
    for (unsigned n = 0; n < 8; ++n, angle += 0x200) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, static_cast<short>(Word(Sc() + 0x2E)));
        PutFloat(p + 0xC, static_cast<short>(Word(Sc() + 0x30)));
        PutFloat(p + 0x18, VS(0));
        PutFloat(p + 0x1C, VS(2));
        v = Sin(angle);
        const auto x = static_cast<std::uint16_t>(static_cast<unsigned>(Shl12(v, 5)) + Word(Sc() + 0x2E));
        SetVW(0, x);
        PutFloat(p + 0x28, static_cast<short>(x));
        v = Cos(angle);
        const auto y = static_cast<std::uint16_t>(static_cast<unsigned>(Shl12(v, 5)) + Word(Sc() + 0x30));
        PutFloat(p + 0x2C, static_cast<short>(y));
        SetVW(2, y);
        p[4] = red;
        p[5] = green;
        p[6] = blue;
        for (unsigned k = 0; k < 3; ++k) {
            p[0x14 + k] = 1;
            p[0x24 + k] = 1;
        }
        s = Sc();
        MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(s + 0x34)), static_cast<unsigned long>(Long(s + 0x38)), 2,
                                    0x34);
    }
}

// original 0x4E43C0: the first of the 64 sparks without bit 0 of +0 gets it; its
// index in al, 0xFF when all are taken.
C2_EXPORT unsigned char __cdecl HolocaustSpark_Alloc(void) { return Alloc(kSparks, 64); }

void MagicC2_Inject() {
    if (bof3::WantsShadow("magic_c2")) magic_c2::SelfTest();
    BOF3_INJECT(BoneDance_Task);
    BOF3_INJECT(BoneDance_Start);
    BOF3_INJECT(BoneDance_Run);
    BOF3_INJECT(BoneDance_Cast);
    BOF3_INJECT(BoneDance_Hold);
    BOF3_INJECT(BoneDance_Spawn);
    BOF3_INJECT(BoneDance_WaitBones);
    BOF3_INJECT(BoneDance_End);
    BOF3_INJECT(BoneDanceChild_Task);
    BOF3_INJECT(BoneDanceBone_Run);
    BOF3_INJECT(BoneDanceBone_Start);
    BOF3_INJECT(BoneDanceBone_Fall);
    BOF3_INJECT(BoneDanceBone_End);
    BOF3_INJECT(BoneDanceShake_Run);
    BOF3_INJECT(BoneDanceShake_Start);
    BOF3_INJECT(BoneDanceShake_Step);
    BOF3_INJECT(BoneDanceShake_Down);
    BOF3_INJECT(BoneDanceShake_Up);
    BOF3_INJECT(BoneDanceFollow_Run);
    BOF3_INJECT(BoneDanceFollow_Start);
    BOF3_INJECT(BoneDanceFollow_Step);
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
    BOF3_INJECT(UtmostAttack_Task);
    BOF3_INJECT(UtmostAttack_Start);
    BOF3_INJECT(UtmostAttack_WaitCaster);
    BOF3_INJECT(UtmostAttack_Stream);
    BOF3_INJECT(UtmostAttack_End);
    BOF3_INJECT(UtmostAttack_ClearStreaks);
    BOF3_INJECT(UtmostAttack_DrawStreaks);
    BOF3_INJECT(UtmostAttack_DrawStreak);
    BOF3_INJECT(UtmostAttack_SpawnStreaks);
    BOF3_INJECT(UtmostAttack_FreeStreak);
    BOF3_INJECT(UtmostAttack_InitStreak);
    BOF3_INJECT(Holocaust_Task);
    BOF3_INJECT(Holocaust_Start);
    BOF3_INJECT(HolocaustBeam_Task);
    BOF3_INJECT(HolocaustBeam_Run);
    BOF3_INJECT(HolocaustBeam_Aim);
    BOF3_INJECT(HolocaustBeam_Grow);
    BOF3_INJECT(HolocaustBeam_Emit);
    BOF3_INJECT(HolocaustBeam_Fade);
    BOF3_INJECT(HolocaustBeam_Draw);
    BOF3_INJECT(HolocaustSpark_Task);
    BOF3_INJECT(HolocaustSpark_Run);
    BOF3_INJECT(HolocaustSpark_Start);
    BOF3_INJECT(HolocaustSpark_Rise);
    BOF3_INJECT(HolocaustSpark_Fade);
    BOF3_INJECT(HolocaustSpark_Draw);
    BOF3_INJECT(HolocaustSpark_Alloc);
}

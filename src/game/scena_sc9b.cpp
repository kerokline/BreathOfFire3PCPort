// Chapter 9's tail and chapter 10 of the scenario code, 0x557170..0x55C040
// (round ten group SC9b). docs/scena_sc9b.md.
//
// Chapter 9's tail (0x557170..0x558140), reached only through tables:
//   - eight Scena09_Objects entries (7..11, 13..15; group SC9a's table, read
//     there in place): a flag or a run started;
//   - vtable 0x6613E8 slot 2 Scena09_StepHook (x, z): by the area, the flags
//     and a rectangle of the position, a run started, al 1; else 0;
//   - slot 4 Scena09_CellHook (x, z): 0x56D800 over Scena09_Cells, then a
//     tail jump through Scena09_CellHooks to one of fourteen cell handlers,
//     each answering in al.
// Chapter 10 (0x558140..0x55C040), vtable 0x661510:
//   - slot 0 Scena10_Frame through Scena10_States (0 is 0x5646B0, group
//     SC13's; 1 Scena10_EnterArea; 2 Scena10_Run through Scena10_Runs);
//   - the runs, each a switch on the step byte 0x8034E5 whose cases wait on a
//     counter, the request byte, the wait word, a timer or an effect, do one
//     thing and set the next step;
//   - slot 1 Scena10_ObjectTrigger through Scena10_Objects (13; entries 11 and
//     12 are also Scena09_Objects 5 and 6); slots 2 and 3 the step and arrive
//     hooks;
//   - the helpers the runs call directly, and two handlers areas 75 and 86
//     name in their descriptors' tables (Scena10_PickupPose,
//     Scena10_PickupEffect).
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. The tables
// are read in place and their entries called directly, as Scena12_Frame
// does; the fuzz swaps them for recorders. No divergence: each function is a
// faithful replacement, except that a dispatcher whose index lies outside its
// table, or whose entry is 0, aborts where the original would jump through
// its neighbour or to address 0 - the project's rule for an index past a
// table (round nine, section 6).
#include "game/scena_sc9b.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/scena_sc9b_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = scena_sc9b::at;
using scena_sc9b::ByteFn;
using scena_sc9b::CellEntry;
using scena_sc9b::CellFindFn;
using scena_sc9b::KeyItemFn;
using scena_sc9b::ObjectEntry;
using scena_sc9b::PlaceFn;
using scena_sc9b::VoidFn;

unsigned char& B(std::uint32_t a) { return *reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }
std::uint16_t& W(std::uint32_t a) { return *reinterpret_cast<std::uint16_t*>(static_cast<std::uintptr_t>(a)); }
std::uint32_t& D(std::uint32_t a) { return *reinterpret_cast<std::uint32_t*>(static_cast<std::uintptr_t>(a)); }
std::int32_t S32(std::uint32_t a) { return static_cast<std::int32_t>(D(a)); }

unsigned Area() { return W(at::kArea); }
unsigned char Counter(unsigned k) { return B(at::kCounters + k); }
void SetCounter(unsigned k, unsigned char v) { B(at::kCounters + k) = v; }
void BumpCounter() { SetCounter(0, static_cast<unsigned char>(Counter(0) + 1)); }
unsigned char Step() { return B(at::kStep); }
void SetStep(unsigned char v) { B(at::kStep) = v; }
void SetRun(unsigned char v) { B(at::kRun) = v; }
void SetPass(unsigned char v) { B(at::kPassFlags) = v; }
unsigned char Request() { return B(at::kRequest); }
void SetRequest(unsigned char v) { B(at::kRequest) = v; }
bool WaitClear() { return W(at::kWait) == 0; }
unsigned char FD() { return B(at::kCondFD); }
void SetFE(unsigned char v) { B(at::kCondFE) = v; }
void SetTimer(std::uint16_t v) { W(at::kTimer) = v; }
// The runs' countdown: a word decrement, true when it reaches 0 (0 wraps).
bool TimerDone() {
    W(at::kTimer) = static_cast<std::uint16_t>(W(at::kTimer) - 1);
    return W(at::kTimer) == 0;
}
// Field_ScriptFlags as the originals touch it: its low byte or'd, the word and'd.
void ScriptOrByte(unsigned char v) { B(at::kScriptFlags) = static_cast<unsigned char>(B(at::kScriptFlags) | v); }
void ScriptAnd(std::uint16_t v) { W(at::kScriptFlags) = static_cast<std::uint16_t>(W(at::kScriptFlags) & v); }
void Redraw() { B(at::kRedraw) = 2; }
unsigned char* SpriteCurrent() { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(D(at::kSpriteCurrent))); }
unsigned char MemberByte(unsigned m, unsigned offset) { return B(at::kObjTrio + m * at::kObjStride + offset); }
unsigned char LeaderKind() { return B(at::kObjTrio + at::kMemberKind); }

// The four counters 0x903848..B zeroed, in that order; counters 1..3 alone.
void ClearCounters() {
    for (unsigned k = 0; k < 4; ++k) SetCounter(k, 0);
}
void ClearCounters123() {
    for (unsigned k = 1; k < 4; ++k) SetCounter(k, 0);
}

// The flag bits are read from 0x929ED0 afresh for every call, as the
// originals load the dword before each push.
unsigned char* Bits() { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(D(at::kFlagBits))); }
bool Flag(unsigned i) { return SH_CALL(Flags_Test)(Bits(), i) != 0; }   // test al, al
void Set(unsigned i) { SH_CALL(Flags_Set)(Bits(), i); }
void Clr(unsigned i) { SH_CALL(Flags_Clear)(Bits(), i); }
// Flags_Set / Flags_Clear on a row handed as a literal, not through 0x929ED0.
void SetAt(std::uint32_t row, unsigned i) { SH_CALL(Flags_Set)(&B(row), i); }
void ClrAt(std::uint32_t row, unsigned i) { SH_CALL(Flags_Clear)(&B(row), i); }

void Set40() { SH_CALL(ScriptFlags_Set40)(); }
void Clear40() { SH_CALL(ScriptFlags_Clear40)(); }
void Msg(unsigned short id) { SH_CALL(Msg_OpenScript)(id); }
void MsgSystem(unsigned id) { SH_CALL(Msg_OpenSystem)(id); }
void ChangeArea(unsigned area, int x, int z, unsigned flags) { SH_CALL(Field_ChangeArea)(area, x, z, flags); }
void CallA(unsigned n) { SH_CALL(Scenario_CallA)(n); }
void CallB(unsigned n) { SH_CALL(Scenario_CallB)(n); }
void DropIn(unsigned e) { SH_CALL(Party_DropIn)(e); }
void Kind2(unsigned char a) { SH_CALL(Kind2_Place)(a); }
void Transition(unsigned char k) { SH_CALL(Transition_Start)(k); }
void Sound(unsigned short id) { SH_CALL(Sound_PlayEffect)(id); }
void MusicPlay(unsigned track, int frames) { SH_CALL(Music_Play)(track, frames); }
void FadeOut(int frames) { SH_CALL(Music_FadeOut)(frames); }
void FadeOutStop(int frames) { SH_CALL(Music_FadeOutStop)(frames); }
void ViewReset() { SH_CALL(Field_ViewReset)(); }
void SetElevation(int v) { SH_CALL(MapView_SetElevation)(v); }
long Elevation(long x, long z) { return SH_CALL(AreaMap_Elevation)(x, z); }
bool LoadDone() { return SH_CALL(File_LoadDone)() != 0; }   // test eax, eax
unsigned char FindFree() { return SH_CALL(Effect_FindFree)(); }
void PartyPlace(int x, int z, unsigned kind) { SH_AT(PlaceFn, at::kPartyPlace)(x, z, kind); }
void SetBit80() { SH_AT(VoidFn, at::kSetBit80)(); }
void Shake() { SH_CALL(Scena10_Shake)(); }

// The chapter's message: Msg_OpenScript(id), the request byte 2.
void Say(unsigned short id) {
    Msg(id);
    SetRequest(2);
}

// A run's end: ScriptFlags_Clear40, then the run and the step 0.
void EndRun() {
    Clear40();
    SetRun(0);
    SetStep(0);
}

unsigned char* Effect(unsigned char slot) { return &B(at::kEffects + static_cast<std::uint32_t>(slot) * at::kEffectStride); }
// The in-use bit of the record a cell names, read without testing the cell
// for 0xFF (as the originals do).
bool EffectBusy(std::uint32_t cell) { return (Effect(B(cell))[0] & 1) != 0; }
bool EffectFree(std::uint32_t cell) { return Effect(B(cell))[0] == 0; }

// An effect of kind 0x13 at a camera-relative point: +0 = 1, +5 = 0x13, the
// dwords +0x64 = x, +0x68 = y, +0x6C = z, the byte +9 = life.
void Place13(unsigned char slot, std::int32_t x, std::int32_t y, std::int32_t z, unsigned char life) {
    unsigned char* const e = Effect(slot);
    e[0] = 1;
    e[5] = 0x13;
    *reinterpret_cast<std::int32_t*>(e + 0x64) = x;
    *reinterpret_cast<std::int32_t*>(e + 0x68) = y;
    *reinterpret_cast<std::int32_t*>(e + 0x6C) = z;
    e[9] = life;
}
std::int32_t AngleX() { return static_cast<std::int16_t>(W(at::kAngleX)); }
std::int32_t AngleY() { return static_cast<std::int16_t>(W(at::kAngleY)); }
std::int32_t AngleFB() { return static_cast<std::int16_t>(W(at::kAngleFB)); }

// Effect_FindFree to `cell`, and on a slot, +0 = 1 and +5 = kind in the record
// the cell names (the originals index by the byte read back). The slot.
unsigned char Spawn(std::uint32_t cell, unsigned char kind) {
    const unsigned char slot = FindFree();
    B(cell) = slot;
    if (slot == 0xFF) return slot;
    unsigned char* const e = Effect(B(cell));
    e[0] = 1;
    e[5] = kind;
    return slot;
}
// Effect_FindFree to the chapter's slot cell, then Place13 on a slot (the
// camera read after the call). The slot.
unsigned char SpawnCamera(std::int32_t x, bool x_from_angle, std::int32_t z, bool z_from_fb, unsigned char life) {
    const unsigned char slot = FindFree();
    B(at::kSlot10) = slot;
    if (slot == 0xFF) return slot;
    Place13(slot, x_from_angle ? AngleX() : x, AngleY(), z_from_fb ? AngleFB() : z, life);
    return slot;
}
// Kind 0x61's record: +0 = 1, +5 = 0x61, +6, +7.
void Spawn61(unsigned char b6, unsigned char b7) {
    const unsigned char slot = Spawn(at::kEffectSlot, 0x61);
    if (slot == 0xFF) return;
    unsigned char* const e = Effect(B(at::kEffectSlot));
    e[6] = b6;
    e[7] = b7;
}
// Kind 0x5D's record at the chapter's slot: +6 = 0x4E, +7 = 3.
void Spawn5D() {
    const unsigned char slot = FindFree();
    B(at::kSlot10) = slot;
    if (slot == 0xFF) return;
    unsigned char* const e = Effect(slot);
    e[0] = 1;
    e[5] = 0x5D;
    e[6] = 0x4E;
    e[7] = 3;
}

// The kind-2 sprite moved: Field_Kind2X / Z and (with `sprite`) Sprite_Kind2's
// +0x34 / +0x38.
void Kind2At(std::int32_t x, std::int32_t z) {
    D(at::kKind2X) = static_cast<std::uint32_t>(x);
    D(at::kSpriteKind2X) = static_cast<std::uint32_t>(x);
    D(at::kKind2Z) = static_cast<std::uint32_t>(z);
    D(at::kSpriteKind2Z) = static_cast<std::uint32_t>(z);
}

// A table entry read in place, the index checked against the table and the
// entry against 0 (the original would call address 0).
std::uint32_t Entry(std::uint32_t table, int index, unsigned count, const char* who) {
    if (index < 0 || static_cast<unsigned>(index) >= count)
        bof3::Fatal("%s: index %d outside its table of %u at 0x%X", who, index, count, table);
    const std::uint32_t e = D(table + 4 * static_cast<std::uint32_t>(index));
    if (e == 0) bof3::Fatal("%s: entry %d of the table at 0x%X is 0 (the original calls address 0)", who, index, table);
    return e;
}

bool In(std::int32_t v, std::int32_t lo, std::int32_t hi) { return v >= lo && v <= hi; }
// A cell word minus a base below a bound, as the originals compare 16-bit
// registers unsigned.
bool Near(std::uint16_t cell, unsigned base, unsigned n) { return static_cast<std::uint16_t>(cell - base) < n; }

// The step hooks' shared hit: ScriptFlags_Set40, the counters (all four, or
// counter 0 alone), the step and the run; al 1.
unsigned char Start(unsigned char step, unsigned char run, bool all_counters) {
    Set40();
    if (all_counters) ClearCounters();
    else SetCounter(0, 0);
    SetStep(step);
    SetRun(run);
    return 1;
}

// Scena09_StepHook's body.
unsigned char StepHook9(std::int32_t x, std::int32_t z) {
    if (Area() == 0x25 && Flag(0x2E) && !Flag(0x38) && In(x, 0x200000, 0x238000) && In(z, 0x410000, 0x418000))
        return Start(0xF, 0xF, true);
    if (Area() == 0x27) {
        if ((x == 0x310000 || x == 0x318000) && In(z, 0x270000, 0x298000)) return Start(0x1E, 0xF, false);
        return 0;
    }
    // area 0x29: the first rectangle's x test is `x == 0x400000 or not above
    // 0x408000` (x <= 0x408000), as the original compares
    if (Area() == 0x29 && Flag(0x38) && !Flag(0x23)) {
        const bool a = x <= 0x408000 && In(z, 0x270000, 0x288000);
        const bool b = In(x, 0x480000, 0x4B8000) && (z == 0x250000 || z == 0x258000);
        const bool c = (x == 0x40000 || x == 0x38000) && In(z, 0x20000, 0x38000);
        if (a || b || c) return Start(0x64, 0xF, false);
    }
    if (Area() == 0x2E && Flag(0x23) && !Flag(0x24) && In(x, 0xF0000, 0x108000) && (z == 0x220000 || z == 0x228000)) {
        Set40();
        ClearCounters123();
        SetStep(0);
        SetCounter(0, 1);
        SetRun(0x10);
        return 1;
    }
    if (Area() == 0x31) {
        if (!Flag(2)) {
            if (In(x, 0x268000, 0x2B8000) && In(z, 0x2B0000, 0x2F8000)) {
                Set(0x25);
                return Start(0, 2, true);
            }
        } else if (!Flag(4) && In(x, 0x290000, 0x2B8000) && In(z, 0x5C0000, 0x5C8000)) {
            return Start(0xA, 2, true);
        }
        if (Flag(0x1E) && !Flag(0x2E) && In(x, 0x4E0000, 0x4E8000) && In(z, 0x220000, 0x258000)) {
            ScriptOrByte(0xE);
            return Start(0, 0xE, false);
        }
    }
    if (Area() == 0x3B && Flag(0x3A)) {
        if (!Flag(0x22)) {
            if (In(x, 0x470000, 0x498000) && In(z, 0x90000, 0xA8000)) return Start(0x82, 0xF, true);
        } else if (!Flag(0x23) && In(x, 0x290000, 0x298000) && In(z, 0x110000, 0x188000)) {
            return Start(0x8C, 0xF, true);
        }
    }
    if (Area() == 0x77) {
        if (Flag(0x3D) && !Flag(0x15) && In(x, 0x420000, 0x448000) && In(z, 0x3C0000, 0x3E8000)) {
            Set(0x15);
            return Start(0x14, 8, true);
        }
        if (Flag(0x3D) && !Flag(0x16) && In(x, 0x420000, 0x448000) && In(z, 0x410000, 0x438000))
            return Start(0x19, 8, true);
        if (!Flag(0x1E)) {
            if (In(x, 0xA0000, 0xA8000) && In(z, 0x20000, 0x30000)) return Start(0, 9, false);
        } else if (!Flag(0x2C) && In(x, 0x60000, 0x68000) && In(z, 0x20000, 0x30000)) {
            return Start(0x1E, 9, false);
        }
    }
    if (Area() != 0x52) return 0;
    if (FD() != 0) return 0;
    // the cell words: the whole parts of x and z (the original reloads them
    // from its arguments' high halves)
    const auto xc = static_cast<std::uint16_t>(static_cast<std::uint32_t>(x) >> 16);
    const auto zc = static_cast<std::uint16_t>(static_cast<std::uint32_t>(z) >> 16);
    if (Flag(0x20) && !Flag(0x21) && Near(zc, 0x40, 5) && Near(xc, 0x22, 4)) {
        Set(0x21);
        Set40();
        SetRun(0xB);
        SetStep(0x1E);
        return 1;
    }
    if (!Near(zc, 0x3C, 5) || !Near(xc, 0x22, 4)) return 0;
    const unsigned char b = B(at::kLeaderByte8);
    B(at::kEffectSlot) = b;
    if (b == 6 || b == 5 || b == 4) return 0;
    if (LeaderKind() == 2) {
        if (Flag(0x20)) return 0;
        Set40();
        SetStep(0);
        SetRun(0xB);
        return 1;
    }
    if (!Flag(0x20) || Flag(0x21)) {
        SetCounter(0, 0x10);
        Set40();
        return 1;
    }
    return 0;
}

// The cell handlers' shared shape: ScriptFlags_Set40, the step, run 6; al 1.
unsigned char Run6At(unsigned char step) {
    Set40();
    SetStep(step);
    SetRun(6);
    return 1;
}
// Cell handlers 0..5: the leader's byte not 5 starts run 6 at 0; else the
// first of three flags clear picks the step.
unsigned char CellLeader(unsigned f1, unsigned char s1, unsigned f2, unsigned char s2, unsigned f3, unsigned char s3) {
    if (LeaderKind() != 5) return Run6At(0);
    if (!Flag(f1)) return Run6At(s1);
    if (!Flag(f2)) return Run6At(s2);
    if (!Flag(f3)) return Run6At(s3);
    return Run6At(5);
}
// Cell handlers 6..10: flag 0x3D clear starts run 8 at 5; else run 8 at
// `step` and flag 0x18 + step.
unsigned char CellRun8(unsigned char step) {
    if (!Flag(0x3D)) {
        Set40();
        SetStep(5);
        SetRun(8);
        return 1;
    }
    Set40();
    SetStep(step);
    SetRun(8);
    Set(0x18u + step);
    return 1;
}

// The three member searches: the first of `n` character bytes at `kinds`
// (read in place) that a member's +0x89 holds answers the word at `ids` of
// its index; none answers `none`.
std::uint16_t MsgByMember(std::uint32_t kinds, std::uint32_t ids, unsigned n, std::uint16_t none) {
    const unsigned count = B(at::kMemberCount);
    for (unsigned k = 0; k < n; ++k) {
        const unsigned char want = B(kinds + k);
        for (unsigned m = 0; m < count; ++m)
            if (MemberByte(m, at::kMemberKind) == want) return W(ids + 2 * k);
    }
    return none;
}

// The pickup record whose (x, z) words equal Sprite_Current's +0x36 / +0x3A
// (the record's words zero-extended against the sprite's sign-extended, as the
// original compares), or -1.
int PickupAt(const unsigned char* sprite) {
    const std::int32_t sx = static_cast<std::int16_t>(*reinterpret_cast<const std::uint16_t*>(sprite + 0x36));
    for (unsigned k = 0; k < at::kPickupCount; ++k) {
        const std::uint32_t rec = at::kPickups + 6 * k;
        if (static_cast<std::int32_t>(W(rec + 2)) != sx) continue;
        if (static_cast<std::int32_t>(W(rec + 4)) ==
            static_cast<std::int16_t>(*reinterpret_cast<const std::uint16_t*>(sprite + 0x3A)))
            return static_cast<int>(k);
    }
    return -1;
}

// The area entry of chapter 10; every way out stores state 2.
void EnterArea10Body() {
    if (Area() == 0x52) {
        if (FD() == 0 && !Flag(4)) Clr(0x33);
        if (FD() == 1 && Flag(0x3D) && !Flag(4)) SH_CALL(Scena10_StartRun1)();
    }
    if (Area() == 0x5E) {
        if (!Flag(0x17) && Flag(0x16)) {
            SetPass(0);
            Set(0x17);
        }
        if (!Flag(0x12)) {
            SH_CALL(AreaMap_SetByte)(0x31, 0x1A, 0);
            SH_CALL(AreaMap_SetByte)(0x32, 0x1A, 0);
        }
    }
    if (Area() == 0x62) {
        if (FD() != 0) return;
        if (Flag(4) && !Flag(8)) {
            CallA(0);
            DropIn(1);
        }
    }
    if (Area() == 0x69) {
        if (FD() == 0 && !Flag(0x35)) SetAt(at::kStoryFlags, 0x4F);
        if (FD() == 1) Set(0x36);
    }
    if (Area() == 0x78 && !Flag(0x3D)) {
        if (Flag(0x39)) {
            if (!Flag(0x3A)) {
                DropIn(2);
            } else if (!Flag(0x3B)) {
                if (FD() == 5) {
                    DropIn(7);
                    ScriptOrByte(0x10);
                } else {
                    ScriptAnd(0xFFEF);
                }
            }
        }
        if (Flag(0x3C)) {
            CallA(5);
            DropIn(6);
            ScriptAnd(0xFFEF);
        }
    }
    if (Area() == 0x79) {
        if (!Flag(0x16) && Flag(0x15)) {
            W(at::kAngleX) = 0xFD14;
            W(at::kAngleFB) = 0x360;
            W(at::kCamDist) = 0x5DC;
            Kind2(0);
        }
        B(at::kLeaderByteB) = 0;
    }
    if (Area() == 0x80) {
        if (FD() != 3) return;
        if (!Flag(0x12) && Flag(0xB)) Set(0xC);
    }
    if (Area() == 0x83 && Flag(0x12)) {
        if (FD() != 0) {
            Sound(0x205);
            B(at::kShaking) = 1;
        } else {
            Sound(0x204);
            B(at::kShaking) = 0;
        }
    }
    if (Area() == 0x84 && FD() == 1 && !Flag(0x14)) {
        Sound(0x204);
        B(at::kShaking) = 1;
        W(at::kAngleFB) = 0x302;
        SpawnCamera(-0x2AA, false, 0x200, false, 0x40);
    }
}

// Run 1's step 7: the 0x2000 colours at 0x80F580 greyed - each channel the
// mean of the three (the sum over 3, a signed divide of a non-negative sum),
// bit 15 kept (Scena12_Run4's step 0x17 does the same).
void GreyClut() {
    std::uint16_t* const c = &W(at::kClut);
    for (unsigned i = 0; i < at::kClutWords; ++i) {
        const unsigned v = c[i];
        const unsigned sum = (((v >> 10) & 0x1F) + ((v >> 5) & 0x1F) + (v & 0x1F)) & 0xFFFF;
        const unsigned g = sum / 3;
        c[i] = static_cast<std::uint16_t>((((g << 5) | g) << 5) | (v & 0x8000) | g);
    }
}

// Run 6's body; the run then calls Scena10_Shake on every way out.
void Run6Body() {
    switch (Step()) {
    case 0:
        B(at::kShaking) = 0;
        Transition(0);
        SetStep(1);
        return;
    case 1:
        if (!WaitClear()) return;
        SetPass(0);
        CallA(1);
        SetStep(2);
        return;
    case 2:
        ChangeArea(0x80, 0x580000, 0x390000, 0x85);
        B(at::kByte937F98) = 0xFF;
        B(at::kByte904EE0) = 0xFF;
        B(at::kMusicCurrent) = 0x88;
        SetStep(3);
        return;
    case 3:
        if (!WaitClear()) return;
        SetPass(0x1F);
        Transition(1);
        SetCounter(0, 1);
        SetStep(4);
        return;
    case 4:
        if (Counter(0) != 6) return;
        FadeOutStop(4);
        Sound(0x204);
        B(at::kShaking) = 1;
        SetTimer(0x1E);
        SetStep(5);
        return;
    case 5:
        if (!TimerDone()) return;
        Transition(0);
        SetStep(6);
        return;
    case 6:
        if (!WaitClear()) return;
        B(at::kShaking) = 0;
        Transition(1);
        D(at::kKind2X) = 0x1F0000;
        D(at::kKind2Z) = 0x360000;
        ViewReset();
        SetStep(7);
        BumpCounter();
        return;
    case 7:
        if (!WaitClear()) return;
        SetStep(8);
        return;
    case 8:
        if (Counter(0) != 8) return;
        SetStep(9);
        return;
    case 9:
        if (Counter(0) != 9) return;
        SpawnCamera(-0x2D6, false, 0x29A, false, 0x80);
        Kind2(1);
        SetStep(0xA);
        return;
    case 0xA:
        if (Counter(0) != 0xA) return;
        if (EffectBusy(at::kSlot10)) return;
        Transition(0);
        SetStep(0xB);
        return;
    case 0xB:
        if (!WaitClear()) return;
        B(at::kShaking) = 1;
        Transition(1);
        Kind2At(0x510000, 0x80000);
        ViewReset();
        W(at::kSpriteKind2Y) = 0xFAA0;
        SetElevation(static_cast<int>(0xFFFFFAA0u));
        SetStep(0xC);
        return;
    case 0xC:
        if (!WaitClear()) return;
        SetStep(0xD);
        BumpCounter();
        return;
    case 0xD:
        if (Counter(0) != 0xD) return;
        Transition(0);
        MusicPlay(0x86, 0x10);
        SetStep(0xE);
        return;
    case 0xE: {
        if (!WaitClear()) return;
        Transition(1);
        Kind2At(0x580000, 0x390000);
        ViewReset();
        const unsigned char slot = FindFree();
        B(at::kSlot10) = slot;
        if (slot != 0xFF) {
            unsigned char* const e = Effect(slot);
            e[0] = 1;
            e[5] = 0x18;
            e[1] = 0x63;
        }
        W(at::kSpriteKind2Y) = 0xF640;
        SetElevation(static_cast<int>(0xFFFFF640u));
        SetStep(0xF);
        return;
    }
    case 0xF:
        if (!WaitClear()) return;
        SetStep(0x10);
        BumpCounter();
        return;
    case 0x10:
        if (Counter(0) != 0x13) return;
        if (SpawnCamera(-0x210, false, 0xE2, false, 0x5A) == 0xFF) return;
        Sound(0x203);
        SetStep(0x11);
        return;
    case 0x11:
        if (EffectBusy(at::kSlot10)) return;
        ScriptOrByte(0x80);
        B(at::kShaking) = 0;
        Sound(0x205);
        ChangeArea(0x79, 0x2A0000, 0x180000, 0x80);
        B(at::kByte937F98) = 1;
        SetStep(0x12);
        return;
    case 0x12:
        if (!WaitClear() || Counter(0) != 1) return;
        CallB(1);
        ChangeArea(0x84, 0x50000, 0x460000, 0x80);
        B(at::kByte904EE0) = 0;
        SetStep(0x13);
        return;
    case 0x13:
        if (!WaitClear() || Counter(0) != 5) return;
        Clear40();
        SetCounter(0, 0);
        Set(0x14);
        SetStep(0x14);
        return;
    case 0x14:
        B(at::kShaking) = FD() != 0 ? 1 : 0;
        return;
    case 0x15:
        if (Counter(0) != 2) return;
        SetPass(0);
        CallA(2);
        ChangeArea(0x84, 0x50000, 0x460000, 0x82);
        B(at::kByte937F98) = 0xFF;
        B(at::kByte904EE0) = 0xFF;
        SetTimer(4);
        SetStep(0x16);
        return;
    case 0x16:
        if (!TimerDone()) return;
        B(at::kShaking) = 1;
        SetPass(0x1F);
        SetStep(0x17);
        return;
    case 0x17:   // and 0x1B: the kind-2 sprite to (0x24, 0x2E), its height the map's
    case 0x1B:
        if (Counter(0) != (Step() == 0x17 ? 3 : 8)) return;
        SetPass(0);
        B(at::kShaking) = 0;
        Kind2At(0x240000, 0x2E0000);
        ViewReset();
        W(at::kSpriteKind2Y) = static_cast<std::uint16_t>(Elevation(S32(at::kKind2X), S32(at::kKind2Z)));
        SetTimer(4);
        SetStep(Step() == 0x17 ? 0x18 : 0x1C);
        return;
    case 0x18:
    case 0x1A:
    case 0x1C:
    case 0x20:
    case 0x2C:
        if (!TimerDone()) return;
        SetPass(0x1F);
        SetStep(static_cast<unsigned char>(Step() + 1));
        BumpCounter();
        return;
    case 0x19:   // and 0x1F, 0x2B: the kind-2 sprite to (5, 0x46)
    case 0x1F:
        if (Counter(0) != (Step() == 0x19 ? 6 : 0xD)) return;
        SetPass(0);
        B(at::kShaking) = 1;
        Kind2At(0x50000, 0x460000);
        ViewReset();
        W(at::kSpriteKind2Y) = static_cast<std::uint16_t>(Elevation(S32(at::kKind2X), S32(at::kKind2Z)));
        SetTimer(4);
        SetStep(static_cast<unsigned char>(Step() + 1));
        return;
    case 0x1D:
        if (Counter(0) != 0xB) return;
        if (SpawnCamera(0, true, 0x10E, false, 0x40) == 0xFF) return;
        SetStep(0x1E);
        return;
    case 0x1E:
        if (EffectBusy(at::kSlot10)) return;
        SetStep(0x1F);
        BumpCounter();
        return;
    case 0x21:
        if (Counter(0) != 0xF) return;
        SetPass(0);
        B(at::kShaking) = 0;
        Kind2At(0x1A0000, 0x110000);
        ViewReset();
        W(at::kSpriteKind2Y) = 0x320;
        SetElevation(0x320);
        W(at::kAngleX) = 0xFCD2;
        W(at::kAngleFB) = 0x74;
        SetTimer(4);
        SetStep(0x22);
        return;
    case 0x22: {
        if (!TimerDone()) return;
        SetPass(0x1F);
        BumpCounter();
        Sound(0x205);
        Sound(0x200);
        FadeOut(8);
        const unsigned char slot = FindFree();
        B(at::kSlot10) = slot;
        if (slot != 0xFF) {
            unsigned char* const e = Effect(slot);
            e[0] = 1;
            e[5] = 0x18;
            e[1] = 0x5F;
        }
        SetFE(0x23);
        SetStep(0x23);
        return;
    }
    case 0x23:
        if (SpawnCamera(-0x3C8, false, 0, true, 0x40) == 0xFF) return;
        SetStep(0x24);
        return;
    case 0x24:
        if (EffectBusy(at::kSlot10)) return;
        SetStep(0x25);
        [[fallthrough]];
    case 0x25:
        if (SpawnCamera(0, true, 0x17C, false, 0x50) == 0xFF) return;
        SetStep(0x26);
        return;
    case 0x26:
        if (EffectBusy(at::kSlot10)) return;
        SetStep(0x27);
        [[fallthrough]];
    case 0x27: {
        const unsigned char slot = FindFree();
        B(at::kSlot10) = slot;
        if (slot == 0xFF) return;
        SetTimer(0x50);
        SetStep(0x28);
        unsigned char* const e = Effect(slot);
        e[0] = 1;
        e[5] = 0x31;
        *reinterpret_cast<std::int32_t*>(e + 0x64) = -0x3F4;
        *reinterpret_cast<std::int32_t*>(e + 0x68) = AngleY();
        *reinterpret_cast<std::int32_t*>(e + 0x6C) = 0x2B0;
        *reinterpret_cast<std::int32_t*>(e + 0xC) = 0x100;
        e[9] = 0xFF;
        return;
    }
    case 0x28:
        if (EffectBusy(at::kSlot10)) return;
        if (!TimerDone()) return;
        Transition(0);
        SetStep(0x29);
        return;
    case 0x29: {
        if (!WaitClear()) return;
        Transition(1);
        Kind2At(0x240000, 0x2F0000);
        ViewReset();
        W(at::kAngleX) = 0xFD56;
        W(at::kAngleFB) = 0x74;
        const unsigned char slot = FindFree();
        B(at::kSlot10) = slot;
        if (slot != 0xFF) {
            SetStep(0x2A);
            Place13(slot, -0x32E, AngleY(), 0x334, 0x40);
        }
        BumpCounter();
        SH_CALL(Sound_ResumeAll)();
        Sound(0x201);
        Sound(0x204);
        return;
    }
    case 0x2A: {
        const unsigned char n = Counter(0);
        SetFE(0x23);
        if (n != 0x13) return;
        Transition(0);
        SetStep(0x2B);
        return;
    }
    case 0x2B:
        if (!WaitClear()) return;
        SetPass(0);
        B(at::kShaking) = 1;
        Kind2At(0x50000, 0x460000);
        ViewReset();
        SetTimer(4);
        SetStep(0x2C);
        return;
    case 0x2D:
        if (Counter(0) != 0x17) return;
        B(at::kShaking) = 0;
        CallA(3);
        Sound(0x205);
        ChangeArea(0x79, 0x2C0000, 0x370000, 0x80);
        B(at::kByte937F98) = 1;
        SetStep(0x2E);
        return;
    case 0x2E:
        if (Counter(0) != 1) return;
        if (SpawnCamera(0, true, 0xA0, false, 0xCC) == 0xFF) return;
        SetStep(0x2F);
        return;
    case 0x2F:
        if (Counter(0) != 2) return;
        Set(0x16);
        ScriptAnd(0xFF7F);
        ChangeArea(0x5E, 0x320000, 0x210000, 0x8D);
        B(at::kByte904EE0) = 0;
        B(at::kByte937F98) = 0xFF;
        SetStep(0x30);
        return;
    case 0x30:
        if (!WaitClear()) return;
        Say(0x96);
        SetStep(0x31);
        return;
    case 0x31:
        if (Request() == 2) return;
        SetPass(0x1F);
        Transition(1);
        BumpCounter();
        SetStep(0x32);
        return;
    case 0x32:
        if (Counter(0) != 0x15) return;
        Clear40();
        SetCounter(0, 0);
        Set(0x17);
        SetRun(0);
        SetStep(0);
        return;
    default: return;
    }
}

// Run 7's body: true when the run returns at once, false when it falls to the
// shared exit (Scena10_Shake if the step, read then, is above 6).
bool Run7Body() {
    switch (Step()) {
    case 0: {
        const unsigned n = B(at::kMemberCount);
        unsigned i = 0;
        while (i < n && MemberByte(i, at::kMemberState) != 3) ++i;
        if (i != n) return false;
        B(at::kShaking) = 0;
        DropIn(0);
        SetStep(1);
        return true;
    }
    case 1: {
        if (Counter(0) != 2) return false;
        Transition(0);
        // the members' +0x89 bytes to 0x903A10.. (the count read after the
        // call; nothing bounds it)
        const unsigned n = B(at::kMemberCount);
        for (unsigned m = 0; m < n; ++m) B(at::kTally + m) = MemberByte(m, at::kMemberKind);
        SetStep(2);
        return true;
    }
    case 2:
        if (!WaitClear()) return false;
        SetPass(0);
        CallB(2);
        SetStep(3);
        return true;
    case 3:
        Set(0x12);
        ChangeArea(0x79, 0x2B0000, 0x390000, 0x80);
        B(at::kByte937F98) = 0xFF;
        B(at::kByte904EE0) = 0xFF;
        B(at::kMusicCurrent) = 0x86;
        SetStep(4);
        return true;
    case 4:
        if (!WaitClear()) return false;
        SetFE(2);
        SetPass(0x1F);
        Transition(1);
        W(at::kAngleX) = 0xFC4E;
        W(at::kAngleFB) = 0x200;
        W(at::kCamDist) = 0x55C;
        DropIn(0);
        Kind2(1);
        SetStep(5);
        return true;
    case 5:
        if (Counter(0) != 2) return false;
        SetStep(6);
        return true;
    case 6: {
        const unsigned char slot = FindFree();
        B(at::kSlot10) = slot;
        if (slot == 0xFF) return false;
        unsigned char* const e = Effect(slot);
        e[0] = 1;
        e[5] = 0x31;
        *reinterpret_cast<std::int32_t*>(e + 0x64) = -0x1A2;
        *reinterpret_cast<std::int32_t*>(e + 0x68) = AngleY();
        *reinterpret_cast<std::int32_t*>(e + 0x6C) = AngleFB();
        *reinterpret_cast<std::int32_t*>(e + 0xC) = -0x2A4;
        e[9] = 0x3E;
        ChangeArea(0x83, 0x70000, 0x5C0000, 0x81);
        B(at::kByte904EE0) = 0xD;
        SetStep(7);
        Shake();
        return true;
    }
    case 7:
        if (!WaitClear() || Counter(0) != 3) return false;
        Clear40();
        SetCounter(0, 0);
        Set(0x18);
        SetStep(9);
        Shake();
        return true;
    case 0xA:
        DropIn(2);
        SetStep(0xB);
        Shake();
        return true;
    case 0xB:
        if (Counter(0) != 2) return false;
        SetStep(0xC);
        Shake();
        return true;
    case 0xC:
        if (!LoadDone()) return false;
        SetStep(0xD);
        BumpCounter();
        Shake();
        return true;
    case 0xD:
        if (Counter(0) != 5) return false;
        Transition(0);
        SetStep(0xE);
        Shake();
        return true;
    case 0xE:
        if (!WaitClear()) return false;
        SetPass(0);
        CallA(4);
        SetStep(0xF);
        Shake();
        return true;
    case 0xF:
        SetCounter(0, 0);
        Set(0x12);
        ChangeArea(0x79, 0x270000, 0x370000, 7);
        B(at::kByte904EE0) = 0xFF;
        Clear40();
        if (Flag(0x1A)) SetAt(at::kRow657, 9);
        SetBit80();
        SetRun(0);
        SetStep(0);
        return true;
    case 0x14:
        if (!WaitClear()) return false;
        Clear40();
        SetPass(0x1F);
        Transition(0xB);
        SetBit80();
        SetRun(0);
        SetStep(0);
        return true;
    default: return false;
    }
}

}  // namespace

// Exported with C linkage (the symbols.gen.h prototypes); no tail calls, so a
// Fatal's stack shows the dispatcher.
#define SC9B_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// Chapter 9's tail: Scena09_Objects entries 7..11 and 13..15 (the entry's
// (object, bits) are not read)

// original 0x557170: object 7 - flag 0xE.
SC9B_EXPORT void __cdecl Scena09_Object07(void) { Set(0xE); }

// original 0x557190: object 8 - run 6 at step 0x64.
SC9B_EXPORT void __cdecl Scena09_Object08(void) {
    Set40();
    SetRun(6);
    SetStep(0x64);
}

// original 0x5571B0: object 9 - flag 0x10.
SC9B_EXPORT void __cdecl Scena09_Object09(void) { Set(0x10); }

// original 0x5571D0: object 10 - flag 0x14.
SC9B_EXPORT void __cdecl Scena09_Object10(void) { Set(0x14); }

// original 0x5571F0: object 11 - run 8 at step 0xF.
SC9B_EXPORT void __cdecl Scena09_Object11(void) {
    Set40();
    SetRun(8);
    SetStep(0xF);
}

// original 0x557210: object 13 - run 0xF at step 0.
SC9B_EXPORT void __cdecl Scena09_Object13(void) {
    Set40();
    SetRun(0xF);
    SetStep(0);
}

// original 0x557230: object 14 - run 0xF at step 0x14.
SC9B_EXPORT void __cdecl Scena09_Object14(void) {
    Set40();
    SetRun(0xF);
    SetStep(0x14);
}

// original 0x557250: object 15 - run 0xF at step 0x28.
SC9B_EXPORT void __cdecl Scena09_Object15(void) {
    Set40();
    SetRun(0xF);
    SetStep(0x28);
}

// original 0x557270: slot 2, the step hook (x, z) - by the area (read again
// before each block after the calls of the block before), two flags and a
// rectangle of the position, a run started and al 1; else al 0.
SC9B_EXPORT unsigned char __cdecl Scena09_StepHook(int x, int z) { return StepHook9(x, z); }

// original 0x557A20: slot 4, the cell hook (x, z) - 0x56D800(Scena09_Cells,
// 14, x, z); a negative answer (as a signed byte) answers 0xFF; else a tail
// jump through Scena09_CellHooks with (x, z) in place, whose al is the answer.
SC9B_EXPORT unsigned char __cdecl Scena09_CellHook(int x, int z) {
    const unsigned char found = SH_AT(CellFindFn, at::kCellFind)(reinterpret_cast<const void*>(at::kCells9), 0xE, x, z);
    const int index = static_cast<signed char>(found);
    if (index < 0) return 0xFF;
    return reinterpret_cast<CellEntry>(static_cast<std::uintptr_t>(
        Entry(at::kCellHooks9, index, at::kCell9Count, "Scena09_CellHook")))(x, z);
}

// original 0x557A50: cell 0 - run 6 at 0 (the leader's byte not 5), else at
// 0x14 (flag 0xC clear) or 5.
SC9B_EXPORT unsigned char __cdecl Scena09_Cell00(void) {
    if (LeaderKind() != 5) return Run6At(0);
    if (!Flag(0xC)) return Run6At(0x14);
    return Run6At(5);
}

// original 0x557AB0: cell 1 - run 6 at 0, or by flags 0xC / 0xE / 0xD at 2 / 3
// / 0x1E, else 5.
SC9B_EXPORT unsigned char __cdecl Scena09_Cell01(void) { return CellLeader(0xC, 2, 0xE, 3, 0xD, 0x1E); }

// original 0x557B70: cell 2 - run 6 at 0, or by flags 0xE / 0x10 / 0xF at 2 /
// 3 / 0x28, else 5.
SC9B_EXPORT unsigned char __cdecl Scena09_Cell02(void) { return CellLeader(0xE, 2, 0x10, 3, 0xF, 0x28); }

// original 0x557C30: cell 3 - run 6 at 0, or by flags 0x10 / 0x12 at 2 / 3,
// flag 0x1D at 0x32 with counter 0 cleared, flag 0x11 at 7, else 5.
SC9B_EXPORT unsigned char __cdecl Scena09_Cell03(void) {
    if (LeaderKind() != 5) return Run6At(0);
    if (!Flag(0x10)) return Run6At(2);
    if (!Flag(0x12)) return Run6At(3);
    if (!Flag(0x1D)) {
        Set40();
        SetCounter(0, 0);
        SetStep(0x32);
        SetRun(6);
        return 1;
    }
    if (!Flag(0x11)) return Run6At(7);
    return Run6At(5);
}

// original 0x557D20: cell 4 - run 6 at 0, or by flags 0xC / 0xE at 2 / 3, flag
// 0xD at 6; with all three set, Cond_ByteFE 2 alone. 1.
SC9B_EXPORT unsigned char __cdecl Scena09_Cell04(void) {
    if (LeaderKind() != 5) return Run6At(0);
    if (!Flag(0xC)) return Run6At(2);
    if (!Flag(0xE)) return Run6At(3);
    if (!Flag(0xD)) return Run6At(6);
    SetFE(2);
    return 1;
}

// original 0x557DD0: cell 5 - run 6 at 0, or by flags 0xD / 0x10 at 2 / 3,
// flag 0xF at 6; with all three set, Cond_ByteFE 3 alone. 1.
SC9B_EXPORT unsigned char __cdecl Scena09_Cell05(void) {
    if (LeaderKind() != 5) return Run6At(0);
    if (!Flag(0xD)) return Run6At(2);
    if (!Flag(0x10)) return Run6At(3);
    if (!Flag(0xF)) return Run6At(6);
    SetFE(3);
    return 1;
}

// original 0x557E80..0x558000: cells 6..10 - flag 0x3D clear: run 8 at 5;
// else run 8 at 0..4 and flag 0x18..0x1C. 1.
SC9B_EXPORT unsigned char __cdecl Scena09_Cell06(void) { return CellRun8(0); }
SC9B_EXPORT unsigned char __cdecl Scena09_Cell07(void) { return CellRun8(1); }
SC9B_EXPORT unsigned char __cdecl Scena09_Cell08(void) { return CellRun8(2); }
SC9B_EXPORT unsigned char __cdecl Scena09_Cell09(void) { return CellRun8(3); }
SC9B_EXPORT unsigned char __cdecl Scena09_Cell10(void) { return CellRun8(4); }

// original 0x558060: cell 11 - flag 0x3D clear: flag 0x3D and sound 0x203;
// else run 8 at 6. 1.
SC9B_EXPORT unsigned char __cdecl Scena09_Cell11(void) {
    if (!Flag(0x3D)) {
        Set(0x3D);
        Sound(0x203);
        return 1;
    }
    Set40();
    SetStep(6);
    SetRun(8);
    return 1;
}

// original 0x5580B0: cell 12 - flag 0x2C clear: run 9 at 0x14, 1; else 0xFF.
SC9B_EXPORT unsigned char __cdecl Scena09_Cell12(void) {
    if (Flag(0x2C)) return 0xFF;
    Set40();
    SetStep(0x14);
    SetRun(9);
    return 1;
}

// original 0x5580E0: cell 13 - flag 0x33 set: 0xFF; else run 0xF at 0x32
// (the leader's byte 4, counter 0 cleared first) or 0x34. 1.
SC9B_EXPORT unsigned char __cdecl Scena09_Cell13(void) {
    if (Flag(0x33)) return 0xFF;
    if (LeaderKind() == 4) {
        SetCounter(0, 0);
        Set40();
        SetStep(0x32);
        SetRun(0xF);
        return 1;
    }
    Set40();
    SetStep(0x34);
    SetRun(0xF);
    return 1;
}

// ===========================================================================
// Chapter 10: the vtable's slots, the state and run dispatchers

// original 0x558140: slot 0, Field_ModeDispatch's call every field frame - a
// tail jump through Scena10_States on the s8 state 0x8034E2: 0 0x5646B0
// (group SC13's block), 1 Scena10_EnterArea, 2 Scena10_Run.
SC9B_EXPORT void __cdecl Scena10_Frame(void) {
    const int state = static_cast<signed char>(B(at::kState));
    reinterpret_cast<VoidFn>(static_cast<std::uintptr_t>(Entry(at::kStates10, state, at::kState10Count, "Scena10_Frame")))();
}

// original 0x5585F0: state 2, a tail jump through Scena10_Runs on the s8 run
// MoveScript_Var7: 0 a bare ret (0x437CC0), 1..7 and 10..13 the runs; 8 and 9
// are 0.
SC9B_EXPORT void __cdecl Scena10_Run(void) {
    const int run = static_cast<signed char>(B(at::kRun));
    reinterpret_cast<VoidFn>(static_cast<std::uintptr_t>(Entry(at::kRuns10, run, at::kRun10Count, "Scena10_Run")))();
}

// original 0x55B6D0: slot 1, called by 0x56D6D0 with the object that
// triggered: Scena10_Objects[object +0x86] (object, the flag bits' pointer).
SC9B_EXPORT void __cdecl Scena10_ObjectTrigger(unsigned char* object) {
    const std::uint32_t bits = D(at::kFlagBits);
    const unsigned index = object[0x86];
    reinterpret_cast<ObjectEntry>(static_cast<std::uintptr_t>(
        Entry(at::kObjects10, static_cast<int>(index), at::kObject10Count, "Scena10_ObjectTrigger")))(object, bits);
}

// original 0x558150: state 1, the chapter's area entry - by the area (read
// again before each block), the flags and Cond_ByteFD: a run started through
// Scena10_StartRun1, map bytes cleared, call-table entries, drop-ins, the
// camera set, the shake flag and a camera effect; every exit stores state 2.
SC9B_EXPORT void __cdecl Scena10_EnterArea(void) {
    EnterArea10Body();
    B(at::kState) = 2;
}

// original 0x558550: called by Scena10_EnterArea in area 0x52 - ScriptFlags_Set40,
// Party_DropIn(0), the kind-2 sprite to (0x49, 0x56), the view reset, the
// camera, a camera effect on the chapter's slot, music file 0x7A, run 1 step 0.
SC9B_EXPORT void __cdecl Scena10_StartRun1(void) {
    Set40();
    DropIn(0);
    D(at::kKind2X) = 0x490000;
    D(at::kKind2Z) = 0x560000;
    ViewReset();
    W(at::kAngleX) = 0xFDC4;
    SpawnCamera(-0x2EC, false, 0x31E, false, 0x80);
    SH_CALL(Music_LoadFile)(0x7A);
    SetRun(1);
    SetStep(0);
}

// ===========================================================================
// Chapter 10's runs (Scena10_Runs entries), each a switch on the step 0x8034E5

// original 0x558600: run 1 - steps 0..0xC: the effect, a load, two
// transitions around an area change into 0x42, the palette greyed, an area
// change into 0x52, a ground effect, flag 6.
SC9B_EXPORT void __cdecl Scena10_Run1(void) {
    switch (Step()) {
    case 0:
        if (EffectBusy(at::kSlot10)) return;
        SetCounter(0, 1);
        SetStep(1);
        return;
    case 1:
        if (Counter(0) != 5) return;
        SetStep(2);
        return;
    case 2:
        if (Counter(0) != 6) return;
        SpawnCamera(-0x2AA, false, 0x200, false, 0x78);
        SetStep(3);
        return;
    case 3:
        if (Counter(0) != 7) return;
        SetStep(4);
        return;
    case 4:
        if (Counter(0) != 9) return;
        if (!LoadDone()) return;
        SetStep(5);
        return;
    case 5:
        if (Counter(0) != 0x19) return;
        Transition(0);
        SetStep(6);
        return;
    case 6:
        if (!WaitClear()) return;
        SetPass(0);
        ChangeArea(0x42, 0x190000, 0x120000, 0x80);
        B(at::kMusicCurrent) = 0xFF;
        SetStep(7);
        return;
    case 7:
        if (!WaitClear()) return;
        GreyClut();
        B(at::kClutDirty) = 1;
        SetStep(8);
        return;
    case 8:
        if (Counter(0) != 1) return;
        SetPass(0x1F);
        Transition(1);
        SetStep(9);
        return;
    case 9:
        if (Counter(0) != 9) return;
        D(at::kKind2X) = 0x160000;
        D(at::kKind2Z) = 0x150000;
        W(at::kF3Divisor) = 0x20;
        SetStep(0xA);
        return;
    case 0xA:
        if (Counter(0) != 0xB) return;
        Set(5);
        ChangeArea(0x52, 0x480000, 0x580000, 0x81);
        SetStep(0xB);
        B(at::kMusicCurrent) = 0xFF;
        return;
    case 0xB: {
        if (Counter(0) != 9) return;
        if (Spawn(at::kEffectSlot, 0x20) != 0xFF) {
            unsigned char* const e = Effect(B(at::kEffectSlot));
            const std::int32_t x = S32(at::kSprite0X);
            const std::int32_t z = S32(at::kSprite0Z);
            *reinterpret_cast<std::int32_t*>(e + 0x34) = x;
            *reinterpret_cast<std::int32_t*>(e + 0x38) = z;
            const std::int32_t h = static_cast<std::int16_t>(Elevation(x, z));
            *reinterpret_cast<std::int32_t*>(e + 0x3C) = static_cast<std::int32_t>(static_cast<std::uint32_t>(h + 0x80) << 16);
        }
        SetStep(0xC);
        return;
    }
    case 0xC:
        if (Counter(0) != 0) return;
        Clear40();
        Set(6);
        SetRun(0);
        SetStep(0);
        return;
    default: return;
    }
}

// original 0x558960: run 2 - steps 0..0x19: a load and a message by the
// party, effects of kinds 0x68 / 0x61 / 0x62 / 0x69 / 0x64 on counts and
// timers, two area changes into 0x62, music 0x7B, flag 8.
SC9B_EXPORT void __cdecl Scena10_Run2(void) {
    switch (Step()) {
    case 0:
        DropIn(0);
        SH_CALL(LoadDatFile)(0x301);
        SetTimer(0x1E);
        SetStep(1);
        return;
    case 1:
        if (!TimerDone()) return;
        Msg(SH_CALL(Scena10_MsgByMemberB)());
        SetRequest(2);
        SetStep(2);
        return;
    case 2:
        if (Request() == 2) return;
        SetCounter(0, 1);
        SetStep(3);
        return;
    case 3: {
        if (Counter(0) != 4) return;
        if (!LoadDone()) return;
        if (Spawn(at::kEffectSlot, 0x68) != 0xFF) {
            unsigned char* const e = Effect(B(at::kEffectSlot));
            e[1] = 0;
            *reinterpret_cast<std::int32_t*>(e + 0x34) = 0x58000;
            *reinterpret_cast<std::int32_t*>(e + 0x38) = 0x58000;
            *reinterpret_cast<std::int32_t*>(e + 0x3C) = 0x2000000;
        }
        SetStep(4);
        SetTimer(0x5A);
        return;
    }
    case 4:
        if (!TimerDone()) return;
        SetStep(5);
        BumpCounter();
        return;
    case 5:
        if (Counter(0) != 8) return;
        ChangeArea(0x62, 0x380000, 0x240000, 1);
        B(at::kMusicCurrent) = 0xFF;
        SetTimer(0x55);
        SetStep(6);
        return;
    case 6:
        if (!WaitClear()) return;
        Spawn61(0, 0);
        SetTimer(0x55);
        SetStep(7);
        return;
    case 7:
        if (!TimerDone()) return;
        MusicPlay(0x7B, 8);
        SetCounter(0, 1);
        SetStep(8);
        return;
    case 8:
        if (Counter(0) != 0xA) return;
        Spawn61(0, 1);
        SetTimer(0x55);
        SetStep(9);
        return;
    case 9:
        if (!TimerDone()) return;
        SetCounter(0, 0xB);
        SetStep(0xA);
        return;
    case 0xA: {
        if (Counter(0) != 0x10) return;
        if (Spawn(at::kEffectSlot, 0x62) != 0xFF) {
            unsigned char* const e = Effect(B(at::kEffectSlot));
            *reinterpret_cast<std::int32_t*>(e + 0x34) = 0x380000;
            *reinterpret_cast<std::int32_t*>(e + 0x38) = 0x260000;
            *reinterpret_cast<std::int32_t*>(e + 0x3C) = 0x2800000;
        }
        SetTimer(0xC8);
        SetStep(0xB);
        FadeOut(0xC8);
        return;
    }
    case 0xB:
        if (!TimerDone()) return;
        FadeOutStop(1);
        SetStep(0xC);
        BumpCounter();
        return;
    case 0xC: {
        if (Counter(0) != 0x13) return;
        if (Spawn(at::kEffectSlot, 0x69) != 0xFF) {
            unsigned char* const e = Effect(B(at::kEffectSlot));
            const std::int32_t x = S32(at::kObjTrioX);
            *reinterpret_cast<std::int32_t*>(e + 0x34) = x;
            const std::int32_t z = S32(at::kObjTrioZ);
            *reinterpret_cast<std::int32_t*>(e + 0x38) = z;
            const std::int32_t h = static_cast<std::int16_t>(Elevation(x, z));
            *reinterpret_cast<std::int32_t*>(e + 0x3C) = static_cast<std::int32_t>(static_cast<std::uint32_t>(h) << 16);
        }
        SetTimer(0x1E);
        SetStep(0xD);
        return;
    }
    case 0xD:
        if (!TimerDone()) return;
        Transition(8);
        SetStep(0xE);
        return;
    case 0xE:
        if (!WaitClear()) return;
        SetPass(0);
        SetTimer(0x3C);
        SetStep(0xF);
        return;
    case 0xF:
        if (!TimerDone()) return;
        Transition(0);
        SetStep(0x10);
        return;
    case 0x10:
        if (!WaitClear()) return;
        Say(0x13);
        SetStep(0x11);
        return;
    case 0x11:
        if (Request() == 2) return;
        SetPass(0x1F);
        Transition(1);
        SetStep(0x12);
        return;
    case 0x12:
        if (!WaitClear()) return;
        SetStep(0x13);
        BumpCounter();
        return;
    case 0x13: {
        if (Counter(0) != 0x16) return;
        if (Spawn(at::kEffectSlot, 0x64) != 0xFF) {
            unsigned char* const e = Effect(B(at::kEffectSlot));
            *reinterpret_cast<std::int32_t*>(e + 0x64) = S32(at::kObjTrioX);
            *reinterpret_cast<std::int32_t*>(e + 0x34) = S32(at::kObjTrioX);
            *reinterpret_cast<std::int32_t*>(e + 0x68) = S32(at::kObjTrioZ);
            const std::int32_t z = S32(at::kObjTrioZ);
            const std::int32_t x = *reinterpret_cast<std::int32_t*>(e + 0x34);
            *reinterpret_cast<std::int32_t*>(e + 0x38) = z;
            const std::int32_t h = static_cast<std::int16_t>(Elevation(x, z));
            const std::int32_t y = static_cast<std::int32_t>(static_cast<std::uint32_t>(h + 0x80) << 16);
            *reinterpret_cast<std::int32_t*>(e + 0x6C) = y;
            *reinterpret_cast<std::int32_t*>(e + 0x3C) = y;
        }
        SetStep(0x14);
        return;
    }
    case 0x14:
        if (Counter(0) != 0x20) return;
        Spawn61(1, 0);
        SetTimer(0x1E);
        SetStep(0x15);
        return;
    case 0x15:
        if (!TimerDone()) return;
        SetStep(0x16);
        BumpCounter();
        return;
    case 0x16:
        if (Counter(0) != 0x22) return;
        Spawn61(1, 1);
        SetTimer(0x1E);
        SetStep(0x17);
        return;
    case 0x17:
        if (!TimerDone()) return;
        SetStep(0x18);
        BumpCounter();
        return;
    case 0x18:
        if (Request() == 2) return;
        if (Counter(0) != 0x25) return;
        ChangeArea(0x62, 0x68000, 0x70000, 0x82);
        SetStep(0x19);
        return;
    case 0x19:
        if (Counter(0) != 8) return;
        Set(8);
        Clear40();
        SetCounter(0, 0);
        SetRun(0);
        SetStep(0);
        return;
    default: return;
    }
}

// original 0x558FC0: run 3 - steps 0..5 and 0xA..0x15: camera effects on the
// chapter's slot, a message by the party (or a wait), an area change into
// 0x80, a key item and a stream, flags 0xA / 0xB.
SC9B_EXPORT void __cdecl Scena10_Run3(void) {
    switch (Step()) {
    case 0:
        SpawnCamera(0, true, 0x160, false, 0x28);
        DropIn(0);
        SetStep(1);
        return;
    case 1: {
        if (Counter(0) != 0xB) return;
        const std::uint16_t id = SH_CALL(Scena10_MsgByMemberC)();
        if (id == 0xFFFF) {
            SetTimer(0xF);
            SetStep(3);
            return;
        }
        Msg(id);
        SetRequest(2);
        SetStep(2);
        return;
    }
    case 2:
        if (Request() == 2) return;
        SetStep(4);
        BumpCounter();
        return;
    case 3:
        if (!TimerDone()) return;
        SetStep(4);
        BumpCounter();
        return;
    case 4:
        if (Counter(0) != 0) return;
        if (SpawnCamera(0, true, 0x200, false, 0x14) == 0xFF) return;
        SetStep(5);
        return;
    case 5:
        if (!EffectFree(at::kSlot10)) return;
        Clear40();
        Set(0xA);
        SetRun(0);
        SetStep(0);
        return;
    case 0xA:
        DropIn(1);
        SetStep(0xB);
        return;
    case 0xB:
        if (Counter(0) != 0xE) return;
        SetTimer(0x1E);
        SetStep(0xC);
        return;
    case 0xC:
        if (!TimerDone()) return;
        ChangeArea(0x80, 0x510000, 0x90000, 0x82);
        B(at::kMusicCurrent) = 0x3F;
        SetStep(0xD);
        return;
    case 0xD:
        if (Counter(0) != 4) return;
        if (SpawnCamera(0, true, 0xF8, false, 0x18) == 0xFF) return;
        SetStep(0xE);
        return;
    case 0xE:
        if (Counter(0) != 5) return;
        if (SpawnCamera(0, true, 0x200, false, 0x18) == 0xFF) return;
        SetStep(0xF);
        return;
    case 0xF:
        if (Counter(0) != 9) return;
        if (SpawnCamera(-0x1B8, false, 0, true, 0x18) == 0xFF) return;
        SetStep(0x10);
        return;
    case 0x10:
        if (Counter(0) != 0xB) return;
        if (SpawnCamera(-0x2AA, false, 0, true, 0x18) == 0xFF) return;
        SetStep(0x11);
        return;
    case 0x11:
        if (Request() == 2) return;
        SH_AT(KeyItemFn, at::kKeyItemAdd)(9);
        SH_AT(VoidFn, at::kMusicStop)();
        SH_CALL(Sound_LoadStream)(2);
        D(at::kTally) = 0;
        D(at::kTallyWord) = 0;
        SetStep(0x12);
        return;
    case 0x12:
        if (SH_CALL(Sound_StreamDone)() == 0) return;
        SH_CALL(Sound_ResumeAll)();
        Spawn5D();
        SetStep(0x13);
        return;
    case 0x13:
        if (!EffectFree(at::kSlot10)) return;
        SetStep(0x14);
        return;
    case 0x14:
        SetStep(0x15);
        BumpCounter();
        return;
    case 0x15:
        if (Counter(0) != 0) return;
        Clear40();
        CallB(0);
        SH_CALL(Scena10_SpriteOp)();
        Set(0xB);
        SetRun(0);
        SetStep(0);
        return;
    default: return;
    }
}

// original 0x559500: run 4 - steps 0..4, 6, 8..0xA, 0x14..0x18, 0x1E, 0x1F
// (MSVC's two-level switch): a message by flag 0xF and the items 0x4E..0x55,
// the tally test, drop-ins, an area change into 0x58 or 0x4B, flags 0xD..0x11.
SC9B_EXPORT void __cdecl Scena10_Run4(void) {
    switch (Step()) {
    case 0:
        SetCounter(0, 0xA);
        SetStep(1);
        return;
    case 1:
        if (Counter(0) != 0xB) return;
        if (!Flag(0xF)) {
            SetStep(2);
            Msg(0x26);
            SetRequest(2);
            return;
        }
        if (SH_CALL(Scena10_HasItem4Eto55)() != 0) {
            SetStep(3);
            Msg(0x28);
            SetRequest(2);
            return;
        }
        SetStep(2);
        Msg(0x27);
        SetRequest(2);
        return;
    case 2:
        if (Request() == 2) return;
        SetCounter(0, 0);
        SetRun(0);
        SetStep(0);
        Clear40();
        return;
    case 3: {
        if (Request() == 2) return;
        const unsigned char slot = FindFree();
        B(at::kSlot10) = slot;
        if (slot == 0xFF) return;
        SetStep(4);
        unsigned char* const e = Effect(slot);
        e[0] = 1;
        e[5] = 0x5E;
        return;
    }
    case 4:
        if (EffectBusy(at::kSlot10)) return;
        if (SH_CALL(Scena10_TallyMet)() != 0) {
            D(at::kTally) = 0;
            D(at::kTallyWord) = 0;
            DropIn(4);
            Set(0x11);
            SetStep(6);
            return;
        }
        SetCounter(0, 0x14);
        SetStep(5);
        return;
    case 6:
        if (Counter(0) != 0) return;
        SetStep(0);
        SetRun(6);
        return;
    case 8:
        if (Counter(0) != 0x15) return;
        Spawn5D();
        SetStep(9);
        return;
    case 9:
        if (!EffectFree(at::kSlot10)) return;
        SetStep(2);
        return;
    case 0xA:
        DropIn(3);
        Set(0xD);
        SetRun(0);
        SetStep(0);
        Clear40();
        return;
    case 0x14:
        Transition(0);
        SetStep(0x15);
        return;
    case 0x15:
        if (!WaitClear()) return;
        SetPass(0);
        SH_CALL(Task_Sleep)(1);
        CallA(0);
        SetRequest(6);
        SetStep(0x16);
        return;
    case 0x16:
        if (Request() != 0) return;
        ChangeArea(0x58, 0x280000, 0x220000, 1);
        B(at::kByte937F98) = 0xFF;
        B(at::kByte904EE0) = 0xFF;
        SetStep(0x17);
        return;
    case 0x17:
        if (Request() != 0) return;
        SetPass(0x1F);
        Transition(0xB);
        SetStep(0x18);
        return;
    case 0x18:
        if (!WaitClear()) return;
        Set(0xE);
        Clear40();
        SetRun(0);
        SetStep(0);
        SetPass(0x1F);
        return;
    case 0x1E:
        if (!Flag(0xF)) {
            Clear40();
            SetRun(0);
            SetStep(0);
            DropIn(7);
            return;
        }
        DropIn(8);
        SetStep(0x1F);
        return;
    case 0x1F:
        if (Counter(0) != 0xE) return;
        Clear40();
        Set(0x10);
        ChangeArea(0x4B, 0x380000, 0x2C8000, 7);
        SetCounter(0, 0);
        SetRun(0);
        SetStep(0);
        return;
    default: return;
    }
}

// original 0x559970: run 5 - steps 0..6, 8..0xA: the music faded, the camera
// pulled in and out, a party placement and message by the party, event
// battle 0x24, an area change into 0x4B, flag 2.
SC9B_EXPORT void __cdecl Scena10_Run5(void) {
    switch (Step()) {
    case 0:
        if (Counter(0) != 0x1F) return;
        FadeOut(0x20);
        SetStep(1);
        return;
    case 1: {
        const auto d = static_cast<std::uint16_t>(W(at::kCamDist) + 0xFFFB);
        W(at::kCamDist) = d;
        if (d != 0xFD80) return;
        FadeOutStop(0xA);
        B(at::kOtSlot) = 4;
        B(at::kSortOnX) = 0;
        Redraw();
        SetStep(2);
        return;
    }
    case 2:
        if (Counter(0) != 0x20) return;
        PartyPlace(0x220000, 0x1A8000, 0x24);
        SetStep(3);
        return;
    case 3:
        if (Counter(0) != 0x21) return;
        SetStep(4);
        return;
    case 4:
        W(at::kCamDist) = static_cast<std::uint16_t>(W(at::kCamDist) + 0xA);
        if (W(at::kCamDist) != 0) return;
        SetStep(5);
        return;
    case 5:
        if (Counter(0) != 0x23) return;
        Msg(SH_CALL(Scena10_MsgByMemberA)());
        SetRequest(2);
        SetStep(6);
        return;
    case 6:
        if (Counter(0) != 0x24) return;
        Sound(0x207);
        SH_CALL(Field_StartEventBattle)(0x24);
        SetStep(7);
        return;
    case 8:
        Set(2);
        ChangeArea(0x4B, 0x198000, 0x188000, 0x86);
        SetCounter(0, 0x28);
        SetStep(9);
        return;
    case 9:
        if (!WaitClear()) return;
        SetStep(0xA);
        return;
    case 0xA:
        if (Counter(0) != 0) return;
        Clear40();
        SetRun(0);
        SetStep(0);
        return;
    default: return;
    }
}

// original 0x559B20: run 6 - steps 0..0x32: transitions and area changes
// between 0x80, 0x79, 0x84 and 0x5E, the kind-2 sprite moved about the map,
// camera effects on the chapter's slot, flags 0x14, 0x16, 0x17; every way out
// calls Scena10_Shake.
SC9B_EXPORT void __cdecl Scena10_Run6(void) {
    Run6Body();
    Shake();
}

// original 0x55AB00: run 7 - steps 0..7, 0xA..0xF, 0x14: the members' state
// tested and their bytes copied, area changes into 0x79 and 0x83, a camera
// effect, flags 0x12 / 0x18; the shared exit calls Scena10_Shake when the
// step (read there) is above 6, and most steps above 6 call it themselves.
SC9B_EXPORT void __cdecl Scena10_Run7(void) {
    if (Run7Body()) return;
    if (Step() > 6) Shake();
}

// original 0x55AF20: run 10 - step 1, the request byte not 2: Party_DropIn(2),
// flag 0x34, the run and step 0.
SC9B_EXPORT void __cdecl Scena10_Run10(void) {
    if (Step() != 1) return;
    if (Request() == 2) return;
    DropIn(2);
    Set(0x34);
    SetRun(0);
    SetStep(0);
}

// original 0x55AF60: run 11 - step 0x23: ScriptFlags_Clear40, Party_DropIn(4),
// counter 0 0x20, the run and step 0.
SC9B_EXPORT void __cdecl Scena10_Run11(void) {
    if (Step() != 0x23) return;
    Clear40();
    DropIn(4);
    SetCounter(0, 0x20);
    SetRun(0);
    SetStep(0);
}

// original 0x55AF90: run 12 - steps 0..5: message 2, a button, the shake,
// the kind-2 sprite's z moved by 7 and back with the F3 divisor from
// Field_MoveSpeeds +3, camera effects on 0x903850, flag 0x35.
SC9B_EXPORT void __cdecl Scena10_Run12(void) {
    switch (Step()) {
    case 0:
        Say(2);
        SetStep(1);
        return;
    case 1:
        if (Request() == 2) return;
        if (W(at::kInputHeld) == 0) return;
        DropIn(0);
        SetTimer(0x1E);
        SetStep(2);
        B(at::kShaking) = 1;
        return;
    case 2: {
        Shake();
        if (!TimerDone()) return;
        ClrAt(at::kStoryFlags, 0x4F);
        const unsigned speed = B(at::kMoveSpeed3);
        W(at::kKind2ZHigh) = static_cast<std::uint16_t>(W(at::kKind2ZHigh) + 7);
        SetFE(1);
        W(at::kF3Divisor) = static_cast<std::uint16_t>(speed << 3);
        if (Spawn(at::kEffectSlot, 0x13) != 0xFF) {
            unsigned char* const e = Effect(B(at::kEffectSlot));
            *reinterpret_cast<std::int32_t*>(e + 0x64) = -0x3B4;
            *reinterpret_cast<std::int32_t*>(e + 0x68) = AngleY();
            *reinterpret_cast<std::int32_t*>(e + 0x6C) = 0x2A6;
            e[9] = 0x40;
        }
        B(at::kShaking) = 0;
        SetStep(3);
        return;
    }
    case 3:
        if (B(at::kHold) != 0) return;
        SetFE(2);
        SetTimer(0x1E);
        SetStep(4);
        return;
    case 4: {
        if (!TimerDone()) return;
        const unsigned speed = B(at::kMoveSpeed3);
        W(at::kKind2ZHigh) = static_cast<std::uint16_t>(W(at::kKind2ZHigh) - 7);
        W(at::kF3Divisor) = static_cast<std::uint16_t>(speed << 3);
        if (Spawn(at::kEffectSlot, 0x13) != 0xFF) {
            unsigned char* const e = Effect(B(at::kEffectSlot));
            *reinterpret_cast<std::int32_t*>(e + 0x64) = -0x2AA;
            *reinterpret_cast<std::int32_t*>(e + 0x68) = AngleY();
            *reinterpret_cast<std::int32_t*>(e + 0x6C) = 0x200;
            e[9] = 0x40;
        }
        SetStep(5);
        return;
    }
    case 5:
        if (B(at::kHold) != 0) return;
        SetFE(0);
        Clear40();
        Set(0x35);
        SetRun(0);
        SetStep(0);
        return;
    default: return;
    }
}

// original 0x55B190: run 13 - steps 0, 1, 5, 8, 0xA, 0xB, 0xF..0x13, 0x15..0x19
// (MSVC's two-level switch): area changes into 0x78, drop-ins, message 0x3A,
// effects 0x43 / 0x6B, event battle 0x23 at the leader, flags 0x38..0x3D.
SC9B_EXPORT void __cdecl Scena10_Run13(void) {
    switch (Step()) {
    case 0:
        if (Counter(0) != 4) return;
        Set(0x38);
        ChangeArea(0x78, 0x208000, 0x738000, 0x81);
        B(at::kMusicCurrent) = 0x7B;
        SetStep(1);
        return;
    case 1:
        if (Counter(0) != 0) return;
        Set(0x39);
        ScriptOrByte(0x10);
        Clear40();
        SetRun(0);
        SetStep(0);
        return;
    case 5:
        if (Request() == 2) return;
        Clear40();
        Set(0x3A);
        ScriptAnd(0xFFEF);
        DropIn(3);
        SetCounter(0, 0);
        SetRun(0);
        SetStep(0);
        return;
    case 8:
        if (Request() == 2) return;
        Set40();
        Set(0x3B);
        ScriptAnd(0xFFEF);
        CallB(3);
        DropIn(4);
        SetRun(0);
        SetStep(0);
        return;
    case 0xA:
        Say(0x3A);
        SetStep(0xB);
        return;
    case 0xB:
        if (Request() == 2) return;
        EndRun();
        return;
    case 0xF:
        SetCounter(0, 0xA);
        DropIn(5);
        SetStep(0x10);
        return;
    case 0x10: {
        if (Counter(0) != 0xE) return;
        const unsigned char slot = FindFree();
        if (slot != 0xFF) {
            unsigned char* const e = Effect(slot);
            e[0] = 1;
            e[5] = 0x43;
        }
        SetTimer(0xB4);
        SetStep(0x11);
        return;
    }
    case 0x11:
        if (!TimerDone()) return;
        SetTimer(1);
        SetStep(0x12);
        BumpCounter();
        return;
    case 0x12:
        if (!TimerDone()) return;
        B(at::kOtSlot) = 4;
        B(at::kSortOnX) = 0;
        BumpCounter();
        Redraw();
        SetStep(0x13);
        return;
    case 0x13:
        if (Counter(0) != 0x13) return;
        PartyPlace(S32(at::kObjTrioX), S32(at::kObjTrioZ), 0x23);
        SH_CALL(Field_StartEventBattle)(0x23);
        SetStep(0x14);
        return;
    case 0x15:
        Set(0x3C);
        ChangeArea(0x78, 0x1F8000, 0x748000, 7);
        B(at::kByte937F98) = 0xFF;
        B(at::kByte904EE0) = 0xFF;
        B(at::kMusicCurrent) = 0x49;
        SetCounter(0, 0);
        SetStep(0x16);
        return;
    case 0x16:
        if (!WaitClear()) return;
        SetPass(0x1F);
        Transition(1);
        SetStep(0x17);
        return;
    case 0x17:
        if (!WaitClear()) return;
        SetStep(0x18);
        return;
    case 0x18: {
        if (Counter(0) != 0xC) return;
        const unsigned char slot = FindFree();
        if (slot != 0xFF) {
            unsigned char* const e = Effect(slot);
            e[0] = 1;
            e[5] = 0x6B;
        }
        SetStep(0x19);
        return;
    }
    case 0x19:
        if (Counter(0) != 0) return;
        Clear40();
        SetRun(0);
        SetStep(0);
        Set(0x3D);
        return;
    default: return;
    }
}

// ===========================================================================
// Chapter 10's helpers, called directly by its runs

// original 0x5594D0: called by run 3 - 0x57CD90 (a free Sprite_Objects index)
// to the word 0x903850; on one, EventOp_0x(Scena10_SpriteScript).
SC9B_EXPORT void __cdecl Scena10_SpriteOp(void) {
    const unsigned char v = SH_AT(ByteFn, at::kSpriteFindFree)();
    W(at::kEffectSlot) = v;
    if (v == 0xFF) return;
    SH_CALL(EventOp_0x)(reinterpret_cast<const unsigned char*>(at::kSpriteScript));
}

// original 0x559900: called by run 4 - Inventory_Count(0, 0x4E + i, 0) for i
// 0..7 until one answers non-zero in al; the last answer.
SC9B_EXPORT unsigned char __cdecl Scena10_HasItem4Eto55(void) {
    unsigned char r = 0;
    for (unsigned i = 0; i < 8; ++i) {
        r = static_cast<unsigned char>(SH_CALL(Inventory_Count)(0, 0x4E + i, 0));
        if (r != 0) break;
    }
    return r;
}

// original 0x559930: called by run 4 - 1 when the bytes 0x903A10, 12, 13, 15,
// 17 are at least 2, 3, 2, 1, 2; else 0.
SC9B_EXPORT unsigned char __cdecl Scena10_TallyMet(void) {
    if (B(at::kTally) < 2) return 0;
    if (B(at::kTally + 2) < 3) return 0;
    if (B(at::kTally + 3) < 2) return 0;
    if (B(at::kTally + 5) < 1) return 0;
    if (B(at::kTally + 7) < 2) return 0;
    return 1;
}

// original 0x55AAD0: with Scena10_Shaking set, MapView_Redraw 2 and
// Camera_ShiftY moved by four times Scena10_ShakeSteps[Frame_Counter & 3].
SC9B_EXPORT void __cdecl Scena10_Shake(void) {
    if (B(at::kShaking) == 0) return;
    const unsigned f = D(at::kFrameCounter) & 3;
    Redraw();
    const auto step = static_cast<std::int16_t>(static_cast<signed char>(B(at::kShakeSteps + f)));
    W(at::kShiftY) = static_cast<std::uint16_t>(W(at::kShiftY) + static_cast<std::uint16_t>(step * 4));
}

// original 0x55B550: called by run 5 - the message id of the first of
// Scena10_MsgKindsA's four member bytes a member holds; 0 for none.
SC9B_EXPORT unsigned short __cdecl Scena10_MsgByMemberA(void) {
    return MsgByMember(at::kMsgKindsA, at::kMsgKindsA + 4, 4, 0);
}

// original 0x55B5D0: called by run 2 - the same over Scena10_MsgKindsB's five;
// 0 for none.
SC9B_EXPORT unsigned short __cdecl Scena10_MsgByMemberB(void) {
    return MsgByMember(at::kMsgKindsB, at::kMsgKindsB + 8, 5, 0);
}

// original 0x55B650: called by run 3 - the same over Scena10_MsgKindsC's three;
// 0xFFFF for none.
SC9B_EXPORT unsigned short __cdecl Scena10_MsgByMemberC(void) {
    return MsgByMember(at::kMsgKindsC, at::kMsgKindsC + 4, 3, 0xFFFF);
}

// ===========================================================================
// Chapter 10's object handlers (Scena10_Objects entries)

// original 0x55B6F0: object 0 - ScriptFlags_Set40, Party_DropIn(1).
SC9B_EXPORT void __cdecl Scena10_Object00(void) {
    Set40();
    DropIn(1);
}

// original 0x55B700: object 1 - ScriptFlags_Set40, Party_DropIn(4).
SC9B_EXPORT void __cdecl Scena10_Object01(void) {
    Set40();
    DropIn(4);
}

// original 0x55B710: object 2 - run 2 at step 0.
SC9B_EXPORT void __cdecl Scena10_Object02(void) {
    Set40();
    SetRun(2);
    SetStep(0);
}

// original 0x55B730: object 3 - run 3 at step 0xA, counter 0 0xA.
SC9B_EXPORT void __cdecl Scena10_Object03(void) {
    Set40();
    SetRun(3);
    SetStep(0xA);
    SetCounter(0, 0xA);
}

// The object's pose set by objects 4, 6 and 7: +1 = 4, +0x84 = 2, +0x83 the
// pose, the word +0x8A 0.
void Pose(unsigned char* object, unsigned char pose) {
    object[1] = 4;
    object[0x84] = 2;
    object[0x83] = pose;
    *reinterpret_cast<std::uint16_t*>(object + 0x8A) = 0;
}

// original 0x55B750: object 4 - Party_DropIn(0), the object posed 5, flag 0xF.
SC9B_EXPORT void __cdecl Scena10_Object04(unsigned char* object, unsigned) {
    DropIn(0);
    Pose(object, 5);
    Set(0xF);
}

// original 0x55B790: object 5 - the pickup record at Sprite_Current's (x, z):
// its item (+1, plus 0x4E) into the sprite's +0xB, the item's name record to
// Text_Records, Inventory_Add(0, item, 1); added: flag 0x20 + the record's
// +0, system message 2, sound 0x106, the object's word +0x8A + 1,
// ScriptFlags_Set40; not: system message 3. The request byte 2.
SC9B_EXPORT void __cdecl Scena10_Object05(unsigned char* object, unsigned) {
    unsigned char* const sprite = SpriteCurrent();
    const int k = PickupAt(sprite);
    if (k < 0) return;
    const std::uint32_t rec = at::kPickups + 6 * static_cast<std::uint32_t>(k);
    sprite[0xB] = static_cast<unsigned char>(B(rec + 1) + 0x4E);
    const unsigned char* const name = SH_CALL(Item_NamePtr)(0, SpriteCurrent()[0xB]);
    for (unsigned i = 0; i < 4; ++i) D(at::kTextRecords + 4 * i) = *reinterpret_cast<const std::uint32_t*>(name + 4 * i);
    if (SH_CALL(Inventory_Add)(0, SpriteCurrent()[0xB], 1) != 0) {
        Set(static_cast<unsigned char>(B(rec) + 0x20));
        MsgSystem(2);
        Sound(0x106);
        auto& w = *reinterpret_cast<std::uint16_t*>(object + 0x8A);
        w = static_cast<std::uint16_t>(w + 1);
        Set40();
        SetRequest(2);
        return;
    }
    MsgSystem(3);
    SetRequest(2);
}

// original 0x55B9E0: object 6 - Party_DropIn(0xA), the object posed 0x1E,
// flag 0x13.
SC9B_EXPORT void __cdecl Scena10_Object06(unsigned char* object, unsigned) {
    DropIn(0xA);
    Pose(object, 0x1E);
    Set(0x13);
}

// original 0x55BA20: object 7 - Party_DropIn(0xB), the object posed 0x1F,
// flag 0x13.
SC9B_EXPORT void __cdecl Scena10_Object07(unsigned char* object, unsigned) {
    DropIn(0xB);
    Pose(object, 0x1F);
    Set(0x13);
}

// original 0x55BA60: object 8 - ScriptFlags_Set40, Party_DropIn(1), flag 0x15,
// the step + 1 (read after the calls), the object's word +0x8A + 1.
SC9B_EXPORT void __cdecl Scena10_Object08(unsigned char* object, unsigned) {
    Set40();
    DropIn(1);
    Set(0x15);
    SetStep(static_cast<unsigned char>(Step() + 1));
    auto& w = *reinterpret_cast<std::uint16_t*>(object + 0x8A);
    w = static_cast<std::uint16_t>(w + 1);
}

// original 0x55BAA0: object 9 - flag 0x19.
SC9B_EXPORT void __cdecl Scena10_Object09(void) { Set(0x19); }

// original 0x55BAC0: object 11 (and Scena09_Objects entry 5) - run 0xA at
// step 1.
SC9B_EXPORT void __cdecl Scena10_Object11(void) {
    Set40();
    SetRun(0xA);
    SetStep(1);
}

// original 0x55BAE0: object 12 (and Scena09_Objects entry 6) - Sprite_Current's
// +0 cleared, run 0xD at step 5.
SC9B_EXPORT void __cdecl Scena10_Object12(void) {
    SpriteCurrent()[0] = 0;
    Set40();
    SetRun(0xD);
    SetStep(5);
}

// ===========================================================================
// The handlers areas 75 and 86 name (their descriptors' +0x34 / +0x3C tables)

// original 0x55B8B0: the pickup record at Sprite_Current's (x, z): with flag
// 0x20 + its +0 set, the sprite's +0 cleared (Sprite_Current read again);
// else Sprite_SetAnimation(its +1) and the sprite's +0x2A cleared.
SC9B_EXPORT void __cdecl Scena10_PickupPose(void) {
    const int k = PickupAt(SpriteCurrent());
    if (k < 0) return;
    const std::uint32_t rec = at::kPickups + 6 * static_cast<std::uint32_t>(k);
    if (Flag(static_cast<unsigned char>(B(rec) + 0x20))) {
        SpriteCurrent()[0] = 0;
        return;
    }
    SH_CALL(Sprite_SetAnimation)(B(rec + 1));
    SpriteCurrent()[0x2A] = 0;
}

// original 0x55B960: Sprite_Current's +0xB kept, Effect_FindFree into it (the
// sprite read again); a slot: kind 0x5D with +6 the kept byte and +7 0, each
// index read again from the sprite; none: the kept byte put back and
// MoveScript_Object's word +0xA less 2.
SC9B_EXPORT void __cdecl Scena10_PickupEffect(void) {
    const unsigned char kept = SpriteCurrent()[0xB];
    const unsigned char slot = FindFree();
    SpriteCurrent()[0xB] = slot;
    unsigned char* const sprite = SpriteCurrent();
    if (sprite[0xB] == 0xFF) {
        sprite[0xB] = kept;
        unsigned char* const mover = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(D(at::kMoveObject)));
        auto& w = *reinterpret_cast<std::uint16_t*>(mover + 0xA);
        w = static_cast<std::uint16_t>(w + 0xFFFE);
        return;
    }
    Effect(sprite[0xB])[0] = 1;
    Effect(sprite[0xB])[5] = 0x5D;
    Effect(sprite[0xB])[6] = kept;
    Effect(sprite[0xB])[7] = 0;
}

// ===========================================================================
// Chapter 10's hooks

// original 0x55BB00: slot 2, the step hook (x, z) - by the area (read again
// before each block), Cond_ByteFD, flags and the position (the whole x, the
// cell words), a run started and al 1; else al 0. Area 0x52 asks group SC7's
// Scena07_PartyHas89State2.
SC9B_EXPORT unsigned char __cdecl Scena10_StepHook(int x, int z) {
    const auto xc = static_cast<std::uint16_t>(static_cast<std::uint32_t>(x) >> 16);
    const auto zc = static_cast<std::uint16_t>(static_cast<std::uint32_t>(z) >> 16);
    if (Area() == 0x4B) {
        if (FD() != 1) return 0;
        if (!Flag(0x10) && x <= 0x40000 && Near(zc, 0x6B, 4)) {
            Set40();
            SetRun(4);
            SetStep(0x1E);
            return 1;
        }
    }
    if (Area() == 0x52) {
        if (FD() != 0) return 0;
        if (Near(zc, 0x3C, 5) && Near(xc, 0x22, 4)) {
            const unsigned char b = B(at::kLeaderByte8);
            B(at::kEffectSlot) = b;
            if (b != 6 && b != 5 && b != 4) {
                if (SH_CALL(Scena07_PartyHas89State2)() == 0) {
                    SetCounter(0, 0x10);
                    Set40();
                    return 1;
                }
                if (!Flag(0x3D) && !Flag(0x33)) {
                    Set(0x33);
                    Set40();
                    SetRun(0xB);
                    SetStep(0x23);
                    return 1;
                }
            }
        }
        // every way the block above does not answer comes here
        if (Flag(0x3D) && Near(zc, 0x23, 2) && x == 0x458000) Set(7);
    }
    if (Area() == 0x78) {
        if (FD() == 0) {
            if (x != 0x58000) return 0;
            if (!Near(zc, 0x11, 4)) return 0;
            if (!Flag(0x37)) {
                const unsigned n = B(at::kMemberCount);
                for (unsigned i = 0; i < n; ++i) {
                    if (MemberByte(i, at::kMemberKind) != 2) continue;
                    Set40();
                    Set(0x37);
                    ChangeArea(0x78, 0x260000, 0x710000, 0x80);
                    B(at::kMusicCurrent) = 0xFF;
                    SetRun(0xD);
                    SetStep(0);
                    return 1;
                }
            }
        }
        if (FD() == 5 && Flag(0x3B) && !Flag(0x3C) && x == 0x2B8000 && Near(zc, 0x74, 2)) {
            Set40();
            SetRun(0xD);
            SetStep(0xA);
            return 1;
        }
    }
    if (Area() != 0x80) return 0;
    if (FD() == 0 && Flag(0xB) && !Flag(0xE) && z == 0x118000 && Near(xc, 0x18, 5)) {
        const unsigned char b = B(at::kLeaderByte8);
        B(at::kEffectSlot) = b;
        if (b == 0 || b == 1 || b == 2) {
            Set40();
            SetRun(4);
            SetStep(0x14);
            return 1;
        }
    }
    if (FD() == 3 && Flag(0xC) && x == 0x520000 && Near(zc, 7, 3)) {
        Set40();
        SetRun(4);
        SetStep(0);
        Clr(0xC);
        return 1;
    }
    return 0;
}

// original 0x55BE70: slot 3, the arrive hook (x, z) - by the area (read again
// before each block), Cond_ByteFD, flags and the position, a run started (or
// counter 0 0xA) and al 1; else al 0. Area 0x78's hit sets run 0xD step 0xF
// and goes on to the area 0x80 test, so it answers 0 unless that hits.
SC9B_EXPORT unsigned char __cdecl Scena10_ArriveHook(int x, int z) {
    const auto xc = static_cast<std::uint16_t>(static_cast<std::uint32_t>(x) >> 16);
    const auto zc = static_cast<std::int16_t>(static_cast<std::uint32_t>(z) >> 16);
    if (Area() == 0x3A && Flag(8) && !Flag(9) && z >= 0x300000 && Near(xc, 0x1C, 5)) {
        Set40();
        Set(9);
        SetAt(at::kStoryFlags, 0x43);
        SetCounter(0, 0xA);
        return 1;
    }
    if (Area() == 0x69) {
        if (FD() != 0) return 0;
        if (!Flag(0x35) && zc > 0x1C) {
            Set40();
            SetRun(0xC);
            SetStep(0);
            return 1;
        }
    }
    if (Area() == 0x78) {
        if (FD() != 5) return 0;
        if (Flag(0x3B) && !Flag(0x3C) && z == 0x748000 && x == 0x250000) {
            Set40();
            SetRun(0xD);
            SetStep(0xF);
        }
    }
    if (Area() != 0x80) return 0;
    if (!Flag(0xA) && z >= 0x2A0000 && Near(xc, 0x20, 5)) {
        Set40();
        SetRun(3);
        SetStep(0);
        return 1;
    }
    if (Flag(0xB) && !Flag(0xD) && z <= 0x370000 && Near(xc, 0x1F, 2)) {
        Set40();
        SetRun(4);
        SetStep(0xA);
        return 1;
    }
    return 0;
}

void ScenaSc9b_Inject() {
    if (bof3::WantsShadow("scena_sc9b")) scena_sc9b::SelfTest();
    BOF3_INJECT(Scena09_Object07);
    BOF3_INJECT(Scena09_Object08);
    BOF3_INJECT(Scena09_Object09);
    BOF3_INJECT(Scena09_Object10);
    BOF3_INJECT(Scena09_Object11);
    BOF3_INJECT(Scena09_Object13);
    BOF3_INJECT(Scena09_Object14);
    BOF3_INJECT(Scena09_Object15);
    BOF3_INJECT(Scena09_StepHook);
    BOF3_INJECT(Scena09_CellHook);
    BOF3_INJECT(Scena09_Cell00);
    BOF3_INJECT(Scena09_Cell01);
    BOF3_INJECT(Scena09_Cell02);
    BOF3_INJECT(Scena09_Cell03);
    BOF3_INJECT(Scena09_Cell04);
    BOF3_INJECT(Scena09_Cell05);
    BOF3_INJECT(Scena09_Cell06);
    BOF3_INJECT(Scena09_Cell07);
    BOF3_INJECT(Scena09_Cell08);
    BOF3_INJECT(Scena09_Cell09);
    BOF3_INJECT(Scena09_Cell10);
    BOF3_INJECT(Scena09_Cell11);
    BOF3_INJECT(Scena09_Cell12);
    BOF3_INJECT(Scena09_Cell13);
    BOF3_INJECT(Scena10_Frame);
    BOF3_INJECT(Scena10_EnterArea);
    BOF3_INJECT(Scena10_StartRun1);
    BOF3_INJECT(Scena10_Run);
    BOF3_INJECT(Scena10_Run1);
    BOF3_INJECT(Scena10_Run2);
    BOF3_INJECT(Scena10_Run3);
    BOF3_INJECT(Scena10_SpriteOp);
    BOF3_INJECT(Scena10_Run4);
    BOF3_INJECT(Scena10_HasItem4Eto55);
    BOF3_INJECT(Scena10_TallyMet);
    BOF3_INJECT(Scena10_Run5);
    BOF3_INJECT(Scena10_Run6);
    BOF3_INJECT(Scena10_Shake);
    BOF3_INJECT(Scena10_Run7);
    BOF3_INJECT(Scena10_Run10);
    BOF3_INJECT(Scena10_Run11);
    BOF3_INJECT(Scena10_Run12);
    BOF3_INJECT(Scena10_Run13);
    BOF3_INJECT(Scena10_MsgByMemberA);
    BOF3_INJECT(Scena10_MsgByMemberB);
    BOF3_INJECT(Scena10_MsgByMemberC);
    BOF3_INJECT(Scena10_ObjectTrigger);
    BOF3_INJECT(Scena10_Object00);
    BOF3_INJECT(Scena10_Object01);
    BOF3_INJECT(Scena10_Object02);
    BOF3_INJECT(Scena10_Object03);
    BOF3_INJECT(Scena10_Object04);
    BOF3_INJECT(Scena10_Object05);
    BOF3_INJECT(Scena10_PickupPose);
    BOF3_INJECT(Scena10_PickupEffect);
    BOF3_INJECT(Scena10_Object06);
    BOF3_INJECT(Scena10_Object07);
    BOF3_INJECT(Scena10_Object08);
    BOF3_INJECT(Scena10_Object09);
    BOF3_INJECT(Scena10_Object11);
    BOF3_INJECT(Scena10_Object12);
    BOF3_INJECT(Scena10_StepHook);
    BOF3_INJECT(Scena10_ArriveHook);
}

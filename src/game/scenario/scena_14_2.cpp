// Moved here by tools/move_funcs.py from scena_sc13.cpp (docs/source-layout-round.md).
#include "game/scenario/scena_14_2.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/record_views.h"
#include "game/scena_sc13.h"
#include "game/scena_sc13_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = scena_sc13::at;

}  // namespace

namespace {

using scena_sc13::ArgFn;

using scena_sc13::ObjectEntry;

using scena_sc13::PlaceFn;

using scena_sc13::ScriptIdFn;

using scena_sc13::VoidFn;

unsigned char& B(std::uint32_t a) { return *reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }

std::uint16_t& W(std::uint32_t a) { return *reinterpret_cast<std::uint16_t*>(static_cast<std::uintptr_t>(a)); }

std::uint32_t& D(std::uint32_t a) { return *reinterpret_cast<std::uint32_t*>(static_cast<std::uintptr_t>(a)); }

unsigned Area() { return Game_AreaNumber; }

unsigned char FD() { return Cond_ByteFD; }

unsigned char Counter(unsigned k) { return B(::bof3::Img(scena_sc13::at::kCounters) + k); }

void SetCounter(unsigned k, unsigned char v) { B(::bof3::Img(scena_sc13::at::kCounters) + k) = v; }

void BumpCounter0() { SetCounter(0, static_cast<unsigned char>(Counter(0) + 1)); }

unsigned char Step() { return MoveScript_PartyRecords[0x65]; }

void SetStep(unsigned char v) { MoveScript_PartyRecords[0x65] = v; }

void SetRun(unsigned char v) { MoveScript_Var7 = v; }

void SetPass(unsigned char v) { Draw_PassFlags = v; }

unsigned char Request() { return Field_Request; }

void SetRequest(unsigned char v) { Field_Request = v; }

bool WaitClear() { return Transition_Busy == 0; }

void ScriptAnd(std::uint16_t v) { Field_ScriptFlags = static_cast<std::uint16_t>(Field_ScriptFlags & v); }

std::uint16_t Timer() { return W(::bof3::Img(scena_sc13::at::kTimer)); }

void SetTimer(std::uint16_t v) { W(::bof3::Img(scena_sc13::at::kTimer)) = v; }

// dec word [0x8034E6] / jne: true while the timer, one less, is not 0.
bool TimerRuns() {
    W(::bof3::Img(scena_sc13::at::kTimer)) = static_cast<std::uint16_t>(W(::bof3::Img(scena_sc13::at::kTimer)) - 1);
    return W(::bof3::Img(scena_sc13::at::kTimer)) != 0;
}

// The flag bits are read from 0x929ED0 afresh for every call, as the
// originals load the dword before each push.
unsigned char* Bits() { return reinterpret_cast<unsigned char*>(Chapter_FlagRow); }

unsigned char* Story() { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(::bof3::Img(Cond_Flags_addr) + 0xa0)); }

bool Flag(unsigned i) { return SH_CALL(Flags_Test)(Bits(), i) != 0; }   // test al, al

void Set(unsigned i) { SH_CALL(Flags_Set)(Bits(), i); }

void Clr(unsigned i) { SH_CALL(Flags_Clear)(Bits(), i); }

void Set40() { SH_CALL(ScriptFlags_Set40)(); }

void Clear40() { SH_CALL(ScriptFlags_Clear40)(); }

void Msg(unsigned short id) { SH_CALL(Msg_OpenScript)(id); }

void ChangeArea(unsigned area, int x, int z, unsigned flags) { SH_CALL(Field_ChangeArea)(area, x, z, flags); }

void CallA(unsigned n) { SH_CALL(Scenario_CallA)(n); }

void DropIn(unsigned n) { SH_CALL(Party_DropIn)(n); }

void Transition(unsigned char k) { SH_CALL(Transition_Start)(k); }

void Sound(unsigned id) { SH_CALL(Sound_PlayEffect)(id); }

void FadeOutStop(int f) { SH_CALL(Music_FadeOutStop)(f); }

void PartyPass() { SH_AT(VoidFn, scena_sc13::at::kPartyPass.v)(); }

void SetBit80() { SH_AT(VoidFn, scena_sc13::at::kSetBit80.v)(); }

// The chapter's message: Msg_OpenScript(id), the request byte 2.
void Say(unsigned short id) {
    Msg(id);
    SetRequest(2);
}

// A run's end with nothing between the call and the stores.
void EndRun() {
    Clear40();
    SetStep(0);
    SetRun(0);
}

void StopRun() {
    SetRun(0);
    SetStep(0);
}

unsigned char* Effect(unsigned slot) { return &B(::bof3::Img(scena_sc13::at::kEffects) + (slot & 0xFFu) * scena_sc13::at::kEffectStride); }

// Effect_FindFree, the slot stored to `cell` (the originals store it before
// testing it). Answers the slot.
unsigned char Take(std::uint32_t cell) {
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    B(cell) = slot;
    return slot;
}

// A slot's record: +0 = 1, +5 = kind.
void Fx(unsigned slot, unsigned char kind) {
    unsigned char* const e = Effect(slot);
    e[0] = 1;
    e[5] = kind;
}

// test byte [effect(slot)], 1
bool EffectBusy(std::uint32_t cell) { return (Effect(B(cell))[0] & 1) != 0; }

// A table entry read in place, the index checked against the table.
std::uint32_t Entry(std::uint32_t table, int index, unsigned count, const char* who) {
    if (index < 0 || static_cast<unsigned>(index) >= count)
        bof3::Fatal("%s: index %d outside its table of %u at 0x%X", who, index, count, table);
    return D(table + 4 * static_cast<std::uint32_t>(index));
}

}  // namespace

// Exported with C linkage (the symbols.gen.h prototypes); no tail calls, so a
// Fatal's stack shows the dispatcher.
#define SC13_EXPORT extern "C" __attribute__((disable_tail_calls))

// A hook's start of a run: ScriptFlags_Set40, the run and the step, al 1.
static unsigned char Hit(unsigned char run, unsigned char step) {
    Set40();
    SetRun(run);
    SetStep(step);
    return 1;
}

// (u16)(v - lo) < n, as the hooks test a cell's range.
static bool In(std::uint32_t v, unsigned lo, unsigned n) { return static_cast<std::uint16_t>(v - lo) < n; }

static unsigned char Spawn(unsigned kind, unsigned x, unsigned y, unsigned z, unsigned life) {
    return SH_CALL(Scena14_SpawnEffect)(kind, x, y, z, life, 0);
}

static unsigned AngleXW() { return static_cast<std::uint16_t>(Camera_Angles[0]); }

static unsigned AngleYW() { return static_cast<std::uint16_t>(Camera_Angles[1]); }

static unsigned AngleFBW() { return W(::bof3::Img(scena_sc13::at::kAngleFB)); }

// The field's effect record at a fixed cell (0x1C8000, 0x268000): +0x38 and
// +0x34 the cell, +0 = 1, +5 = kind, +0x3C = (the elevation there + 0x80)
// << 16 (the elevation's low word sign-extended).
static void FxAtCell(unsigned slot, unsigned char kind) {
    unsigned char* const e = Effect(slot);
    *reinterpret_cast<std::uint32_t*>(e + 0x38) = 0x268000;
    const std::uint32_t z = *reinterpret_cast<std::uint32_t*>(e + 0x38);
    *reinterpret_cast<std::uint32_t*>(e + 0x34) = 0x1C8000;
    const std::uint32_t x = *reinterpret_cast<std::uint32_t*>(e + 0x34);
    e[0] = 1;
    e[5] = kind;
    const std::int32_t h = static_cast<std::int16_t>(SH_CALL(AreaMap_Elevation)(static_cast<long>(x), static_cast<long>(z)));
    *reinterpret_cast<std::uint32_t*>(e + 0x3C) = static_cast<std::uint32_t>(h + 0x80) << 16;
}

// Scena14_EnterArea's call-table entry with a party placement: CallA(n),
// DropIn(d), 0x532ED0(x, z, kind).
static void JoinAt(unsigned n, unsigned d, int x, int z, unsigned kind) {
    CallA(n);
    DropIn(d);
    SH_AT(PlaceFn, scena_sc13::at::kPartyPlace.v)(x, z, kind);
}

static bool Pending1314() { return Flag(0x13) && !Flag(0x14); }

// original 0x5646C0: state 1, by the area; area 0xAC at Cond_ByteFD not 0
// jumps straight to the end. Every way out stores state 2.
static void EnterArea14Body() {
    if (Area() == 0xB && Pending1314()) {
        SetPass(0);   // the al of the Flags_Test just made, 0
        CallA(3);
        DropIn(5);
        Camera_Angles[0] = 0xFB9E;
        W(::bof3::Img(scena_sc13::at::kAngleFB)) = 0x3B8;
        SH_CALL(Scena14_GreyClut)();
    }
    if (Area() == 0x2B && Pending1314()) JoinAt(4, 5, 0x150000, 0x190000, 0x22);
    if (Area() == 0x67 && Pending1314()) JoinAt(5, 1, 0x4A0000, 0x220000, 0x1E);
    if (Area() == 0x78 && Pending1314()) {
        CallA(6);
        DropIn(8);
    }
    if (Area() == 0x8D) {
        if (!Flag(0x14)) {
            Camera_Distance = 0x7F00;
            CallA(9);
            DropIn(0);
            Spawn(0x31, AngleXW(), AngleYW(), AngleFBW(), 0xF0);
            Sound(0x200);
        }
        if (FD() == 1 && !Flag(0x18)) {
            Set(0x18);
            DropIn(4);
            SH_AT(VoidFn, scena_sc13::at::kArea141c.v)();
        }
    }
    if (Area() == 0x8E) {
        if (!Flag(0x1B)) {
            Set(0x1B);
            CallA(0xA);
        }
        if (FD() == 2 && !Flag(0x1D)) {
            Set(0x1D);
            DropIn(1);
        }
        if (FD() == 3 && !Flag(0x1E)) {
            Set(0x1E);
            DropIn(2);
            SetRun(7);
            SetStep(6);
        }
    }
    if (Area() == 0x90 && Pending1314()) JoinAt(7, 9, 0x288000, 0x1D8000, 0x25);
    if (Area() == 0x93 && !Flag(0x35)) Set(0x35);
    if (Area() == 0x96) {
        if (!Flag(0xA)) {
            if (FD() == 0) {
                CallA(1);
                DropIn(0);
            }
            Cond_ByteFD = 3;
            Cond_ByteFE = 1;
        } else if (!Flag(0xB) && FD() == 0) {
            CallA(2);
            DropIn(2);
            SetPass(0);
            SetTimer(0x3C);
        }
    }
    if (Area() == 0xAC) {
        if (FD() != 0) return;
        if (!Flag(0x12)) {
            const unsigned y = AngleYW();
            Camera_Angles[0] = 0xFD56;
            W(::bof3::Img(scena_sc13::at::kAngleFB)) = 6;
            Camera_Distance = 0x900;
            Spawn(0x13, 0xFD56, y, 0x3A2, 0xE7);
        } else if (Flag(0x13)) {
            if (!Flag(0x14)) {
                SetPass(0x1F);
                CallA(8);
                DropIn(8);
            } else if (!Flag(0x1F)) {
                Camera_Distance = 0x200;
            } else if (!Flag(0x30)) {
                CallA(0xC);
                DropIn(0xB);
                Set(0x30);
                const unsigned char slot = SH_CALL(Effect_FindFree)();   // a slot of its own
                if (slot != 0xFF) FxAtCell(slot, 0x98);
            }
        }
    }
    if (Area() == 0xBD && Flag(1)) Effect_Objects[0].flags = static_cast<unsigned char>(Effect_Objects[0].flags | 0x40);
    if (Area() == 0xBF) {
        if (FD() == 0 && Flag(5)) {
            if (!Flag(6)) {
                Camera_Distance = 0x600;
                Spawn(0x31, AngleXW(), AngleYW(), AngleFBW(), 0xE8);
                SH_CALL(Scenario_CallB)(0);
                DropIn(0);
            } else if (!Flag(7)) {
                Set(7);
                DropIn(2);
            }
        }
        if (FD() == 1 && Flag(5) && Flag(8)) {
            Set(9);
            Clr(5);
            DropIn(5);
            Field_Kind2X = 0x468000;
            Field_Kind2Z = 0xD8000;
            SH_CALL(Field_ViewReset)();
            SetRun(2);
            SetStep(0x1E);
        }
    }
    if (Area() == 0xC1 && !Flag(0)) {
        Set(0);
        CallA(0);
        DropIn(0);
    }
    if (Area() != 0xC5) return;
    if (!Flag(0xD)) {
        Set(0xD);
        DropIn(1);
        SetRun(5);
        SetStep(0);
    }
    if (B(::bof3::Img(scena_sc13::at::kKind2X) + 4) == 5 && !Flag(0xF)) {   // the byte 0x905E68
        DropIn(2);
        SetRun(5);
        SetStep(0xA);
    }
}

static void Shake4() { SH_CALL(Scena14_Shake)(4); }

// The three party objects' +0x48 bytes (ObjTrio records 0..2) set to 1.
static void TrioFlags48() {
    ObjTrio[2].scale_mode = 1;
    ObjTrio[1].scale_mode = 1;
    ObjTrio[0].scale_mode = 1;
}

static void Scroll() { SH_CALL(Scena14_ScrollView)(); }

static void Talk(std::uint32_t who, std::uint32_t lines) {
    SH_CALL(Scena14_TalkByMember)(reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(who)),
                                  reinterpret_cast<const std::uint16_t*>(static_cast<std::uintptr_t>(lines)));
}

// Run 6's four repeated handlers (the jump table holds each at several steps):
// once the wait word is 0, the step + 1 read afresh after the call.
static void NextStep() { SetStep(static_cast<unsigned char>(Step() + 1)); }

static void GreyIn() {   // 0x566873
    if (!WaitClear()) return;
    SetPass(0x1F);
    SH_CALL(Scena14_GreyClut)();
    Transition(1);
    NextStep();
}

static void Hold3C() {   // 0x5668A6
    if (!WaitClear()) return;
    SetTimer(0x3C);
    NextStep();
}

static void HoldThenFade() {   // 0x5668CC
    if (TimerRuns()) return;
    Transition(0);
    NextStep();
}

static void AfterMessage() {   // 0x566922
    if (Request() == 2) return;
    NextStep();
}

static void Hold(std::uint16_t t, unsigned char next) {
    if (!WaitClear()) return;
    SetTimer(t);
    SetStep(next);
}

static void Recall(unsigned short msg, unsigned char next) {
    if (!WaitClear()) return;
    SetPass(0);
    Say(msg);
    SetStep(next);
}

static void Visit(unsigned area, int x, int z, unsigned flags, unsigned char next) {
    ChangeArea(area, x, z, flags);
    ModeFlow_AreaTransition = 0xFF;
    Area_EnterTransition = 0xFF;
    AreaW1f_MusicCurrent = 0xFF;
    SetStep(next);
}

// Steps 0x40..0x42: an effect of kind 0x94 with +1 = 3, the slot to
// Scena14_Slot.
static void Spawn94() {
    const unsigned char slot = Take(::bof3::Img(Scena14_Slot_addr));
    if (slot == 0xFF) return;
    Fx(slot, 0x94);
    Effect(slot)[1] = 3;
}

static void TrioOr40(unsigned k) { B(::bof3::Img(scena_sc13::at::kTrio) + k * scena_sc13::at::kTrioStride) = static_cast<unsigned char>(B(::bof3::Img(scena_sc13::at::kTrio) + k * scena_sc13::at::kTrioStride) | 0x40); }

// Run 7's first six steps: ScriptFlags_Clear40, a flag, a drop-in, an area's
// own object set-up where there is one, and the run's end.
static void Farewell(unsigned flag, unsigned member, std::uint32_t setup) {
    Clear40();
    Set(flag);
    DropIn(member);
    if (setup != 0) SH_WORD(VoidFn, setup)();
    StopRun();
}

// Step 0x17's two copies: records 1..7 of the eight at 0x903A70 (+0xB bit 0
// set: +0x1C = +0x2E, +0x18 = +0x20, +0x1A = +0x22, +0x10 = 0), then for the
// members after the leader each ObjTrio record + 0x80 gets the 0xA4 bytes of
// the record Character_IndexById[party list byte] names.
static void RestoreRecords() {
    for (unsigned r = 1; r < 8; ++r) {
        const std::uint32_t p = ::bof3::Img(CharacterRecords_addr) + r * scena_sc13::at::kRecordStride;
        auto* const c = rv::As<records::CharRecord>(&B(p));   // CharacterRecords 0x903A70, stride 0xA4
        if ((c->flags & 1) == 0) continue;
        c->survive = c->stats.byte_0e;
        c->hp = c->stats.max_hp;
        c->ap = c->stats.max_ap;
        c->status = 0;
    }
    const unsigned char n = Field_MemberCount;
    scena_sc13::at::kSlotWord.b_00 = 1;
    if (n <= 1) return;
    scena_sc13::at::kSlotWord.b_00 = n;
    for (unsigned j = 0; j + 1 < n; ++j) {
        const unsigned char c = B(::bof3::Img(scena_sc13::at::kPartyList) + 1 + j);
        const unsigned char idx = B(::bof3::Img(scena_sc13::at::kEffectState) + c);
        const unsigned char* const src = &B(::bof3::Img(scena_sc13::at::kRecords) + static_cast<std::uint32_t>(idx) * scena_sc13::at::kRecordStride);
        unsigned char* const dst = &B(::bof3::Img(scena_sc13::at::kTrio) + (j + 1) * scena_sc13::at::kTrioStride + 0x80);
        for (unsigned k = 0; k < scena_sc13::at::kRecordStride; ++k) dst[k] = src[k];
    }
}

// Scena14_Objects entries 0..4: the line area code picks for `who` - in area
// 0xC0 0x42C0A0(who), elsewhere 0x42BA90(who) (each an index into its own
// five-entry table, area 192's and area 191's) - opened, Field_Request 2.
static void AreaLine(unsigned who) {
    const unsigned id = Area() == 0xC0 ? SH_AT(ScriptIdFn, scena_sc13::at::kArea192.v)(who) : SH_AT(ScriptIdFn, scena_sc13::at::kArea191.v)(who);
    Msg(static_cast<unsigned short>(id));
    SetRequest(2);
}

// original 0x5646A0: slot 0 - a tail jump through Scena14_States on the s8
// state 0x8034E2: 0 ScenaShared_State0, 1 Scena14_EnterArea, 2 Scena14_Run.
SC13_EXPORT void __cdecl Scena14_Frame(void) {
    const int state = static_cast<signed char>(MoveScript_PartyRecords[0x62]);
    reinterpret_cast<VoidFn>(static_cast<std::uintptr_t>(Entry(::bof3::Img(Scena14_States_addr), state, scena_sc13::at::kStateCount14, "Scena14_Frame")))();
}

// original 0x5646B0: state 0 of chapters 5, 9, 10, 12, 14, 15, 18 and 19 (the
// eight state tables that hold it) - the state byte 1.
SC13_EXPORT void __cdecl ScenaShared_State0(void) { MoveScript_PartyRecords[0x62] = 1; }

SC13_EXPORT void __cdecl Scena14_EnterArea(void) {
    EnterArea14Body();
    MoveScript_PartyRecords[0x62] = 2;
}

// original 0x564E10: Gfx_ClutStrip's 0x2000 colours greyed - each the mean of
// its three 5-bit channels (the sum / 3, rounded toward 0) in all three, bit 15
// kept - and Gfx_ClutStripDirty 1.
SC13_EXPORT void __cdecl Scena14_GreyClut(void) {
    std::uint16_t* const c = &Gfx_ClutStrip[0];
    for (unsigned i = 0; i < scena_sc13::at::kClutWords; ++i) {
        const std::uint32_t w = c[i];
        const std::int32_t sum = static_cast<std::int32_t>(((w >> 10) & 0x1F) + ((w >> 5) & 0x1F) + (w & 0x1F));
        const std::uint32_t grey = static_cast<std::uint32_t>(sum / 3);
        c[i] = static_cast<std::uint16_t>((((grey << 5) | grey) << 5) | (w & 0x8000) | grey);
    }
    Gfx_ClutStripDirty = 1;
}

// original 0x564E80: an effect record by Effect_FindFree, the slot to
// Scena14_Slot; for a slot, +0 = 1, +5 = kind, the dwords +0x64 / +0x68 /
// +0x6C = the sign-extended words x / y / z, +0xC = w, +9 = life, and al 1;
// none al 0.
SC13_EXPORT unsigned char __cdecl Scena14_SpawnEffect(unsigned kind, unsigned x, unsigned y, unsigned z, unsigned life,
                                                      std::uint32_t w) {
    const unsigned char slot = Take(::bof3::Img(Scena14_Slot_addr));
    if (slot == 0xFF) return 0;
    unsigned char* const e = Effect(slot);
    e[0] = 1;
    e[5] = static_cast<unsigned char>(kind);
    *reinterpret_cast<std::int32_t*>(e + 0x64) = static_cast<std::int16_t>(x);
    *reinterpret_cast<std::int32_t*>(e + 0x68) = static_cast<std::int16_t>(y);
    *reinterpret_cast<std::int32_t*>(e + 0x6C) = static_cast<std::int16_t>(z);
    *reinterpret_cast<std::uint32_t*>(e + 0xC) = w;
    e[9] = static_cast<unsigned char>(life);
    return 1;
}

// original 0x564EF0: state 2, a tail jump through Scena14_Runs on the s8 run:
// 0, 8 and 9 a bare ret (0x437CC0), 1..7 Run1..Run7.
SC13_EXPORT void __cdecl Scena14_Run(void) {
    const int run = MoveScript_Var7;
    reinterpret_cast<VoidFn>(static_cast<std::uintptr_t>(Entry(::bof3::Img(Scena14_Runs_addr), run, scena_sc13::at::kRunCount14, "Scena14_Run")))();
}

// original 0x564F00: run 1, steps 0..7, 0x14, 0x19 (the byte table 0x56521C
// of 0x1A, then 10 cases). A choice with Menu_DrawHand while a message is up.
SC13_EXPORT void __cdecl Scena14_Run1(void) {
    const unsigned char step = Step();
    switch (step) {
    case 0:
        if (Request() == 2) return;
        SetCounter(0, 0);
        DropIn(1);
        SetStep(1);
        return;
    case 1: {
        if (Counter(0) != 1) return;
        Set(1);
        ChangeArea(0xBD, 0x10000000, 0x14000000, 2);
        const unsigned char bits = Field_StatusBits;
        AreaW1f_MusicCurrent = Music_Track;
        B(::bof3::Img(scena_sc13::at::kMusicTrack) + 0x22) = 2;   // 0x904153
        Field_StatusBits = static_cast<unsigned char>(bits | 1);
        W(::bof3::Img(0x90405C)) = 0;
        SetStep(2);
        return;
    }
    case 2:
        if (!WaitClear()) return;
        Say(0x12);
        SetStep(3);
        return;
    case 3:
        if (Request() == 2) {
            SH_CALL(Menu_DrawHand)(0x98, 0x28, 0);
            return;
        }
        Spawn(0x13, AngleXW(), AngleYW(), 0xFE00, 0x20);
        SetStep(4);
        return;
    case 4:
        if (EffectBusy(::bof3::Img(Scena14_Slot_addr))) return;
        Say(0x13);
        SetStep(5);
        return;
    case 5:
        if (Request() == 2) {
            SH_CALL(Menu_DrawHand)(0x98, 0x38, 0);
            return;
        }
        Spawn(0x13, AngleXW(), AngleYW(), 0x400, 0x20);
        SetStep(6);
        return;
    case 6:
        if (EffectBusy(::bof3::Img(Scena14_Slot_addr))) return;
        Say(0x14);
        SetStep(7);
        return;
    case 7: {
        if (Request() == 2) {
            SH_CALL(Menu_DrawHand)(0x98, 0x10, 0);
            return;
        }
        Clear40();
        Clr(1);
        ChangeArea(0xC1, 0x1A8000, 0x1C0000, 0x82);
        const unsigned char bits = static_cast<unsigned char>(Field_StatusBits & 0xFE);
        StopRun();
        Field_StatusBits = bits;
        return;
    }
    case 0x14:
    case 0x19: {
        if (Request() == 2) return;
        if (step == 0x14) {
            SH_CALL(Flags_Clear)(Story(), 0x77);
            B(::bof3::Img(scena_sc13::at::kMusicTrack) + 0x21) = 0;   // 0x904152
            ChangeArea(0xC1, 0x1A8000, 0x1C0000, 0x83);
        } else {
            Set(2);
            Set(3);
            ChangeArea(0xC1, 0x1A8000, 0x1C0000, 0x84);
            ModeFlow_AreaTransition = 0xFF;
        }
        SH_CALL(Flags_Set)(Story(), 0x8A);
        const unsigned char bits = static_cast<unsigned char>(Field_StatusBits & 0xFE);
        Area_EnterTransition = 0xFF;
        AreaW1f_MusicCurrent = 0xFF;
        StopRun();
        Field_StatusBits = bits;
        return;
    }
    default:
        return;
    }
}

// original 0x565240: run 2, steps 0..8, 0xA..0x11, 0x14, 0x15, 0x19, 0x1D,
// 0x1E, 0x20..0x24 - a camera pull-out (Camera_Distance from the timer), a
// shaking wait, an event battle, and the 0xBD trip.
SC13_EXPORT void __cdecl Scena14_Run2(void) {
    switch (Step()) {
    case 0:
        if (Counter(0) != 2) return;
        SetTimer(0xF0);
        TrioFlags48();
        SetStep(1);
        return;
    case 1: {
        if (!TimerRuns()) {
            Camera_Distance = 0x76C;
            SH_AT(PlaceFn, scena_sc13::at::kPartyPlace.v)(0x248000, 0x168000, 0x25);
            SetStep(2);
            MapView_Redraw = 2;
            TrioFlags48();
            return;
        }
        MapView_Redraw = 2;
        const std::int32_t d = 0xF0 - static_cast<std::int32_t>(Timer());
        Camera_Distance = static_cast<std::uint16_t>(static_cast<std::int32_t>(d * 1900) / 240);
        return;
    }
    case 2:
        if (Counter(0) != 4) return;
        Cond_ByteFE = 1;
        Sound(0x202);
        SetTimer(0x1E);
        SetStep(3);
        return;
    case 3:
        if (Timer() != 0) {
            SetTimer(static_cast<std::uint16_t>(Timer() - 1));
            Shake4();
            return;
        }
        SetStep(4);
        BumpCounter0();
        return;
    case 4:
        if (Counter(0) != 6) return;
        Sound(0x202);
        SetTimer(0x3C);
        SetStep(5);
        return;
    case 5:
        if (Timer() != 0) {
            SetTimer(static_cast<std::uint16_t>(Timer() - 1));
            Shake4();
            return;
        }
        Cond_ByteFE = 2;
        SetStep(6);
        BumpCounter0();
        return;
    case 6:
        if (Counter(0) != 8) return;
        Cond_ByteFE = 3;
        Sound(0x200);
        SetTimer(0xD2);
        SetStep(7);
        return;
    case 7: {
        if (Timer() == 0) {
            Camera_Distance = 0;
            SetStep(8);
            BumpCounter0();
            MapView_Redraw = 2;
            return;
        }
        SetTimer(static_cast<std::uint16_t>(Timer() - 1));
        Shake4();
        const std::uint16_t t = Timer();
        MapView_Redraw = 2;
        if (t >= 0xB4) return;
        Camera_Distance = static_cast<std::uint16_t>(static_cast<std::int32_t>(t * 1900) / 180);
        return;
    }
    case 8:
        Sound(0x201);
        SH_AT(PlaceFn, scena_sc13::at::kPartyPlace.v)(0x188000, 0x168000, 0x31);
        SH_CALL(Field_StartEventBattle)(0x31);
        SetStep(9);
        return;
    case 0xA:
        ChangeArea(0xBF, 0x1E8000, 0x228000, 1);
        Set(4);
        Set(5);
        SetStep(0xB);
        return;
    case 0xB:
        if (!WaitClear()) return;
        SetStep(0xC);
        return;
    case 0xC:
        if (Counter(0) != 1) return;
        Transition(0);
        SetStep(0xD);
        return;
    case 0xD:
        if (!WaitClear()) return;
        SetPass(0);
        Say(0x70);
        SetStep(0xE);
        return;
    case 0xE:
        if (Request() == 2) return;
        SetStep(0xF);
        return;
    case 0xF:
        ChangeArea(0xBF, 0x490000, 0xD0000, 0x81);
        ModeFlow_AreaTransition = 0xFF;
        Area_EnterTransition = 0xFF;
        SetStep(0x10);
        return;
    case 0x10:
        if (!WaitClear()) return;
        SetPass(0x1F);
        Transition(1);
        SetStep(0x11);
        return;
    case 0x11: {
        if (Counter(0) != 0) return;
        const std::uint32_t r = static_cast<std::uint32_t>(Character_IndexById[8]) * scena_sc13::at::kRecordStride;
        B(::bof3::Img(scena_sc13::at::kRecords) + r + 0x1E) = 9;
        W(::bof3::Img(scena_sc13::at::kRecords) + r + 0x18) = 1;
        Clear40();
        Set(6);
        StopRun();
        return;
    }
    case 0x14:
        Say(0x7D);
        SetStep(0x15);
        return;
    case 0x15:
        if (Request() == 2) return;
        EndRun();
        return;
    case 0x19:
        if (rv::Bytes(ObjTrio)[0x137] != 0) return; // 0x802E77
        Clear40();
        DropIn(3);
        Set(8);
        SH_CALL(Inventory_Add)(0, 0x5A, 1);
        StopRun();
        return;
    case 0x1D:
        Clear40();
        Set(9);
        DropIn(4);
        StopRun();
        return;
    case 0x1E: {
        if (Counter(0) != 5) return;
        const unsigned char slot = Take(::bof3::Img(Scena14_Slot_addr));
        if (slot != 0xFF) Fx(slot, 0x91);
        SetStep(0x1F);
        return;
    }
    case 0x20: {
        ChangeArea(0xBD, 0x18000000, 0xC000000, 2);
        const unsigned char bits = static_cast<unsigned char>(Field_StatusBits | 1);
        B(::bof3::Img(scena_sc13::at::kMusicTrack) + 0x22) = 2;   // 0x904153
        Field_StatusBits = bits;
        W(::bof3::Img(0x90405C)) = 0;
        SetStep(0x21);
        return;
    }
    case 0x21:
        if (!WaitClear()) return;
        SetStep(0x22);
        return;
    case 0x22:
        if (!TimerRuns()) {
            Say(4);
            SetStep(0x23);
        }
        ObjTrio[0].step_frames = static_cast<unsigned char>(ObjTrio[0].step_frames + 1);
        return;
    case 0x23:
        if (Request() != 2) SetStep(0x24);
        ObjTrio[0].step_frames = static_cast<unsigned char>(ObjTrio[0].step_frames + 1);
        return;
    case 0x24: {
        ChangeArea(0x96, 0x1C8000, 0x148000, 5);
        const unsigned char bits = static_cast<unsigned char>(Field_StatusBits & 0xFE);
        ModeFlow_AreaTransition = 0;
        Field_StatusBits = bits;
        SetRun(3);
        SetStep(0);
        return;
    }
    default:
        return;
    }
}

// original 0x565880: the camera shaken - MapView_Redraw 2, Camera_ShiftY +=
// Scena14_ShakeOffsets[Frame_Counter & 3] * amount (16-bit).
SC13_EXPORT void __cdecl Scena14_Shake(unsigned amount) {
    MapView_Redraw = 2;
    const std::int32_t offset = static_cast<signed char>(B(::bof3::Img(scena_sc13::at::kShake14) + (Frame_Counter & 3)));
    Camera_ShiftY = static_cast<std::uint16_t>(static_cast<std::uint16_t>(Camera_ShiftY) + offset * static_cast<std::int32_t>(amount & 0xFF));
}

// original 0x5658B0: no table or call reaches it (below) - flags bit 0 of the
// bytes 0x802EB0 / 0x802FFC (ObjTrio records 1 and 2 + 0x24) cleared,
// Field_ChangeArea(0xC4, 0x298000, 0x168000, 0x80), ScriptFlags_Set40, run 2
// step 0, Field_StatusBits bit 0 cleared.
SC13_EXPORT void __cdecl Scena14_LeaveToC4(void) {
    ObjTrio[1].draw_flags = static_cast<unsigned char>(ObjTrio[1].draw_flags & 0xFE);
    ObjTrio[2].draw_flags = static_cast<unsigned char>(ObjTrio[2].draw_flags & 0xFE);
    ChangeArea(0xC4, 0x298000, 0x168000, 0x80);
    Set40();
    const unsigned char bits = static_cast<unsigned char>(Field_StatusBits & 0xFE);
    SetRun(2);
    SetStep(0);
    Field_StatusBits = bits;
}

// original 0x565910: run 3, steps 0..0xC.
SC13_EXPORT void __cdecl Scena14_Run3(void) {
    switch (Step()) {
    case 0:
        if (Counter(0) != 2) return;
        ChangeArea(0x96, 0x3C8000, 0x378000, 0x81);
        SetStep(1);
        return;
    case 1:
        if (Counter(0) != 8) return;
        Set(0xA);
        ChangeArea(0x96, 0x158000, 0x230000, 3);
        Area_EnterTransition = 0xFF;
        ModeFlow_AreaTransition = 4;
        SetStep(2);
        return;
    case 2:
        if (!WaitClear()) return;
        SetStep(3);
        return;
    case 3:
        if (TimerRuns()) return;
        Transition(1);
        BumpCounter0();
        SetPass(0x1F);
        SetStep(4);
        return;
    case 4:
        if (Counter(0) != 0xA) return;
        ChangeArea(0x96, 0x3C8000, 0xD0000, 0x83);
        SetStep(5);
        return;
    case 5:
        if (Counter(0) != 0x18) return;
        Set(0xB);
        ChangeArea(0x96, 0x1A0000, 0x190000, 0x84);
        SetStep(6);
        return;
    case 6:
        if (Counter(0) != 0x1B) return;
        ChangeArea(0x96, 0x3B0000, 0x368000, 0x85);
        SetStep(7);
        return;
    case 7:
        if (Counter(0) != 0x24) return;
        Transition(0);
        SetStep(8);
        return;
    case 8:
        if (!WaitClear()) return;
        SetPass(0);
        Say(0x15);
        SetStep(9);
        return;
    case 9:
        if (Request() == 2) return;
        B(::bof3::Img(scena_sc13::at::kRecords) + static_cast<std::uint32_t>(Character_IndexById[8]) * scena_sc13::at::kRecordStride + 0x1E) = 0;
        PartyPass();
        SH_CALL(Task_Sleep)(1);
        SetRequest(6);
        SetStep(0xA);
        return;
    case 0xA:
        if (Request() != 0) return;
        Set40();
        SetCounter(0, 0);
        Set(0xC);
        ChangeArea(0x96, 0x3C0000, 0x390000, 1);
        Area_EnterTransition = 0xFF;
        ModeFlow_AreaTransition = 0xFF;
        SetStep(0xB);
        return;
    case 0xB:
        if (Request() != 0) return;
        SetPass(0x1F);
        Transition(1);
        SetStep(0xC);
        return;
    case 0xC:
        if (!WaitClear()) return;
        ScriptAnd(0xFFF7);
        Clear40();
        StopRun();
        return;
    default:
        return;
    }
}

// original 0x565BF0: run 4, steps 0..2, 0xA..0x18. Its ebx holds 0x18 from
// the start: step 0xB waits on counter 3 (0x90384B) at 0x18 and step 0x17
// stores it as the next step (below).
SC13_EXPORT void __cdecl Scena14_Run4(void) {
    switch (Step()) {
    case 0:
        SetCounter(0, 0);
        Set(0x10);
        ChangeArea(0x94, 0x180000, 0x268000, 0x82);
        SetStep(1);
        return;
    case 1:
        if (Counter(0) != 1) return;
        if (Spawn(0x13, 0xFCBC, AngleYW(), 0x284, 0x98) == 0) return;
        SetStep(2);
        return;
    case 2:
        if (Counter(0) != 2) return;
        if (Spawn(0x13, 0xFD56, AngleYW(), 0x200, 0x98) == 0) return;
        StopRun();
        return;
    case 0xA:
        DropIn(3);
        SetStep(0xB);
        return;
    case 0xB:
        if (Counter(3) != 0x18) return;   // cmp [0x90384B], bl - bl is the switch bound 0x18
        Transition(0);
        SetStep(0xC);
        return;
    case 0xC:
        if (!WaitClear()) return;
        SetPass(0);
        Say(0x14);
        SetStep(0xD);
        return;
    case 0xD:
        if (Request() == 2) return;
        SetStep(0xE);
        return;
    case 0xE:
        SH_CALL(Flags_Set)(Story(), 0x46);
        ChangeArea(0x95, 0x1C0000, 0xC0000, 0x80);
        ModeFlow_AreaTransition = 0xFF;
        Area_EnterTransition = 0xFF;
        AreaW1f_MusicCurrent = 0x27;
        SetStep(0xF);
        return;
    case 0xF: {
        if (!WaitClear()) return;
        const unsigned char slot = Take(::bof3::Img(Scena14_Slot_addr));
        if (slot != 0xFF) Fx(slot, 0x8D);
        Scroll();
        SetPass(0x1F);
        Transition(1);
        SetStep(0x10);
        return;
    }
    case 0x10:
        if (!WaitClear()) return;
        Scroll();
        Msg(1);
        SetRequest(2);
        SetStep(0x11);
        Effect(Scena14_Slot[0])[1] = 2;
        return;
    case 0x11:
        Scroll();
        return;
    case 0x12:
        if (Request() != 2 && Spawn(0x13, 0xFB46, AngleYW(), 0x360, 0x78) != 0) {
            const std::int32_t v = MoveScript_F3Divisor;
            SetTimer(0x50);
            MoveScript_F3Divisor = static_cast<std::uint16_t>(v / 2);
            SetStep(0x13);
        }
        Scroll();
        return;
    case 0x13:
        if (!TimerRuns()) {
            Transition(0);
            SetStep(0x14);
        }
        Scroll();
        return;
    case 0x14:
        if (!WaitClear()) return;
        SetPass(0);
        Msg(2);
        SetRequest(2);
        SetStep(0x15);
        return;
    case 0x15:
        if (Request() == 2) return;
        SetStep(0x16);
        return;
    case 0x16:
        SetCounter(3, 0x20);
        ChangeArea(0xA7, 0x280000, 0x630000, 0x8A);
        ModeFlow_AreaTransition = 0xFF;
        Area_EnterTransition = 0xFF;
        AreaW1f_MusicCurrent = 0xFF;
        SetStep(0x17);
        return;
    case 0x17:
        if (!WaitClear()) return;
        Transition(1);
        SetPass(0x1F);
        SH_CALL(Music_LoadFile)(0x8C);
        SetStep(0x18);   // ebx, the switch bound
        return;
    case 0x18:
        if (Counter(3) != 0x31) return;
        if (SH_CALL(File_LoadDone)() == 0) return;
        SetCounter(3, static_cast<unsigned char>(Counter(3) + 1));
        Set(0x11);
        StopRun();
        return;
    default:
        return;
    }
}

// original 0x566000: with MapView_FocusX at exactly 0x77FF, the view moves
// east by 0x14 cells: MapView_Origin + 0x14, the kind-2 sprite's +0x34 and
// Field_Kind2X + 0x140000, the focus 0x63FF, the kind-2 sprite's elevation
// +0x3E from AreaMap_Elevation and MapView_SetElevation(it), the view shift
// 0x56FCA0, and Sprite_ObjectsExtra record 0's +0x34 + 0x140000 with its
// elevation +0x3E.
SC13_EXPORT void __cdecl Scena14_ScrollView(void) {
    auto* const kind2 = &Sprite_Kind2;
    auto* const xobj0 = Sprite_ObjectsExtra;
    if (static_cast<std::uint32_t>(MapView_FocusX) != 0x77FF) return;
    const std::uint32_t kx = static_cast<std::uint32_t>(kind2->x);
    const std::uint32_t fx = static_cast<std::uint32_t>(Field_Kind2X);
    MapView_Origin[0] = static_cast<std::uint16_t>(static_cast<std::uint16_t>(MapView_Origin[0]) + 0x14);
    kind2->x = kx + 0x140000;
    const std::uint32_t fz = static_cast<std::uint32_t>(Field_Kind2Z);
    MapView_FocusX = 0x63FF;
    Field_Kind2X = fx + 0x140000;
    const long h = SH_CALL(AreaMap_Elevation)(static_cast<long>(fx + 0x140000), static_cast<long>(fz));
    kind2->height_whole = static_cast<std::uint16_t>(h);
    SH_CALL(MapView_SetElevation)(static_cast<int>(h));
    SH_AT(VoidFn, scena_sc13::at::kViewShift.v)();
    const std::uint32_t ex = static_cast<std::uint32_t>(xobj0->x) + 0x140000;
    const std::uint32_t ez = static_cast<std::uint32_t>(xobj0->z);
    xobj0->x = ex;
    xobj0->height_whole = static_cast<std::uint16_t>(SH_CALL(AreaMap_Elevation)(static_cast<long>(ex), static_cast<long>(ez)));
}

// original 0x566090: run 5, steps 0..2, 5..7, 0xA..0xE, 0x10, 0x14, 0x15,
// 0x1E..0x21 (the byte table 0x566438 of 0x22, then 19 cases).
SC13_EXPORT void __cdecl Scena14_Run5(void) {
    switch (Step()) {
    case 0:
        if (Counter(0) != 1) return;
        if (Spawn(0x13, 0xFCE8, AngleYW(), 0x150, 0x70) == 0) return;
        SetStep(1);
        return;
    case 1:
        if (Counter(0) != 2) return;
        if (Spawn(0x13, 0xFD56, AngleYW(), 0x200, 0x40) == 0) return;
        SetStep(2);
        return;
    case 2:
        if (Counter(0) != 0) return;
        EndRun();
        return;
    case 5:
        if (Request() == 2) return;
        SH_CALL(Kind2_Place)(1);
        SetStep(6);
        return;
    case 6:
        if (Counter(0) != 1) return;
        Set(0xE);
        SetTimer(0x78);
        SetStep(7);
        return;
    case 7:
        if (TimerRuns()) return;
        SetCounter(0, 2);
        SetStep(2);
        return;
    case 0xA:
        if (Counter(0) != 0x1C) return;
        SH_AT(PlaceFn, scena_sc13::at::kPartyPlace.v)(0xB8000, 0x728000, 0x25);
        SetStep(0xB);
        return;
    case 0xB:
        if (Counter(0) != 0x1E) return;
        SetTimer(0x10);
        TrioFlags48();
        SetStep(0xC);
        return;
    case 0xC: {
        if (TimerRuns()) {
            Camera_Distance = static_cast<std::uint16_t>(static_cast<std::uint16_t>(Camera_Distance) + 0x58);
            MapView_Redraw = 2;
            Shake4();
            return;
        }
        const unsigned char slot = SH_CALL(Effect_FindFree)();   // a slot of its own
        if (slot != 0xFF) Fx(slot, 0x8E);
        SetTimer(0x64);
        SetStep(0xD);
        Shake4();
        return;
    }
    case 0xD:
        if (!TimerRuns()) {
            SetTimer(0x10);
            SetStep(0xE);
            BumpCounter0();
        }
        Shake4();
        return;
    case 0xE:
        if (TimerRuns()) {
            Camera_Distance = static_cast<std::uint16_t>(static_cast<std::uint16_t>(Camera_Distance) - 0x58);
            MapView_Redraw = 2;
            return;
        }
        SH_CALL(Field_StartEventBattle)(0x32);
        SetStep(0xF);
        return;
    case 0x10:
        Clear40();
        ScriptAnd(0xFFF7);
        SetCounter(0, 0);
        Set(0xF);
        StopRun();
        return;
    case 0x14:
        Say(0x1C);
        SetStep(0x15);
        return;
    case 0x15:
        if (Request() == 2) return;
        EndRun();
        return;
    case 0x1E:
        SetCounter(0, 0);
        DropIn(3);
        SetStep(0x1F);
        return;
    case 0x1F:
        if (Counter(0) != 1) return;
        SH_AT(VoidFn, scena_sc13::at::kSound587B80.v)();
        SH_CALL(Sound_LoadStream)(2);
        SH_AT(ArgFn, scena_sc13::at::kParty591900.v)(0xD);
        Say(0x1D);
        SetStep(0x20);
        return;
    case 0x20:
        if (SH_CALL(Sound_StreamDone)() == 0) return;
        if (Request() == 2) return;
        SH_CALL(Sound_ResumeAll)();
        SetStep(0x21);
        BumpCounter0();
        return;
    case 0x21:
        if (Counter(0) != 0) return;
        EndRun();
        return;
    default:
        return;
    }
}

// original 0x566460: run 6, steps 0..7, 0xA..0x49 (one jump table of 74) - a
// talk, a battle's lead-in, then a chain of visits (area, a greyed CLUT,
// a hold, a fade, a message) through areas 0x33, 0x43, 0x55, 0x2B, 0x67,
// 0x78, 0x90 and back to 0xAC.
SC13_EXPORT void __cdecl Scena14_Run6(void) {
    switch (Step()) {
    case 0:
        Set40();
        DropIn(0);
        SetStep(1);
        return;
    case 1:
        if (Counter(3) != 0x14) return;
        SH_CALL(Flags_Set)(Story(), 0x4E);
        ChangeArea(0xAC, 0x260000, 0x360000, 0x84);
        SetStep(2);
        return;
    case 2:
        if (!WaitClear()) return;
        if (EffectBusy(::bof3::Img(Scena14_Slot_addr))) return;
        Talk(::bof3::Img(Scena14_TalkWho_addr), ::bof3::Img(Scena14_TalkLines_addr));
        SetStep(3);
        return;
    case 3:
        if (Request() == 2) return;
        SetCounter(0, 0xA);
        SetStep(4);
        return;
    case 4:
        if (Counter(0) != 0xB) return;
        if (Spawn(0x31, 0xFD56, AngleYW(), 0x200, 0x1E) == 0) return;
        SetStep(5);
        return;
    case 5:
        if (EffectBusy(::bof3::Img(Scena14_Slot_addr))) return;
        BumpCounter0();
        Talk(::bof3::Img(Scena14_TalkWho_addr) + 4, ::bof3::Img(Scena14_TalkLines_addr) + 8);
        SetStep(6);
        return;
    case 6:
        if (Request() == 2) return;
        BumpCounter0();
        SetStep(7);
        return;
    case 7:
        if (Counter(0) != 0) return;
        Set(0x12);
        Clear40();
        StopRun();
        return;
    case 0xA:
        SetCounter(0, 0xF);
        DropIn(5);
        SetStep(0xB);
        return;
    case 0xB: {
        if (Counter(0) != 0x12) return;
        const unsigned char n = Field_MemberCount;
        SetStep(0xC);
        scena_sc13::at::kSlotWord.b_00 = 0;
        for (unsigned char m = 0; m < n;) {
            if (B(::bof3::Img(scena_sc13::at::kLeaderName) + m * scena_sc13::at::kTrioStride) == 4) {
                DropIn(6);
                return;
            }
            ++m;
            scena_sc13::at::kSlotWord.b_00 = m;
        }
        DropIn(7);
        return;
    }
    case 0xC: {
        if (Counter(0) != 0x23) return;
        const unsigned char slot = Take(::bof3::Img(Scena14_Slot_addr));
        if (slot != 0xFF) Fx(slot, 0x94);
        SetTimer(0x5A);
        SetStep(0xD);
        return;
    }
    case 0xD:
        if (TimerRuns()) return;
        SetStep(0xE);
        BumpCounter0();
        return;
    case 0xE:
        if (Counter(0) != 0) return;
        Set(0x13);
        ChangeArea(0xB, 0x1E0000, 0xB0000, 0);
        Area_EnterTransition = 0xFF;
        SetStep(0xF);
        return;
    case 0xF:
        if (Counter(0) != 1) return;
        SetPass(0x1F);
        Transition(1);
        SetStep(0x10);
        return;
    case 0x10:
        if (WaitClear()) {
            SetStep(0x11);
            BumpCounter0();
        }
        [[fallthrough]];
    case 0x11:
        if (Counter(0) != 3) return;
        Spawn(0x13, 0xFD56, AngleYW(), 0x200, 0x80);
        SetStep(0x12);
        return;
    case 0x12:
        if (Counter(0) != 0) return;
        Transition(0);
        SH_CALL(Music_FadeOut)(0x20);
        SetStep(0x13);
        return;
    case 0x13:
        if (!WaitClear()) return;
        FadeOutStop(0xA);
        SetPass(0);
        Say(0x30);
        SetStep(0x14);
        return;
    case 0x14:
        if (Request() == 2) return;
        SetStep(0x15);
        return;
    case 0x15: Visit(0x33, 0x30000, 0x70000, 0x82, 0x16); return;
    case 0x16: case 0x1C: case 0x22: case 0x28: case 0x2E: case 0x34: case 0x3A: GreyIn(); return;
    case 0x17: case 0x1D: case 0x23: case 0x29: Hold3C(); return;
    case 0x18: case 0x1E: case 0x24: case 0x2A: case 0x30: case 0x36: case 0x3C: HoldThenFade(); return;
    case 0x1A: case 0x20: case 0x26: case 0x2C: case 0x32: case 0x38: case 0x3E: AfterMessage(); return;
    case 0x19: Recall(0x27, 0x1A); return;
    case 0x1B: Visit(0x43, 0x1E0000, 0x360000, 0x86, 0x1C); return;
    case 0x1F: Recall(0x41, 0x20); return;
    case 0x21: Visit(0x55, 0x268000, 0x438000, 0x83, 0x22); return;
    case 0x25: Recall(0x14, 0x26); return;
    case 0x27: Visit(0x2B, 0x110000, 0x190000, 0, 0x28); return;
    case 0x2B: Recall(0x27, 0x2C); return;
    case 0x2D: Visit(0x67, 0x4C0000, 0x1F0000, 0, 0x2E); return;
    case 0x2F: Hold(0x32, 0x30); return;
    case 0x31: Recall(0xA, 0x32); return;
    case 0x33: Visit(0x78, 0x1F8000, 0x748000, 0, 0x34); return;
    case 0x35: Hold(0x28, 0x36); return;
    case 0x37: Recall(0x49, 0x38); return;
    case 0x39: Visit(0x90, 0x230000, 0x1D8000, 0, 0x3A); return;
    case 0x3B: Hold(0x1E, 0x3C); return;
    case 0x3D: Recall(0x49, 0x3E); return;
    case 0x3F:
        ChangeArea(0xAC, 0x68000, 0x2C8000, 0);
        ModeFlow_AreaTransition = 0xFF;
        AreaW1f_MusicCurrent = 8;
        SetStep(0x40);
        return;
    case 0x40:
        if (Counter(0) != 0x14) return;
        Spawn94();
        TrioOr40(1);
        TrioOr40(2);
        Sound(0x208);
        SetStep(0x41);
        return;
    case 0x41:
        if (Counter(0) != 0x15) return;
        Spawn94();
        Sound(0x208);
        SetStep(0x42);
        return;
    case 0x42:
        if (Counter(0) != 0x16) return;
        Spawn94();
        TrioOr40(0);
        Sound(0x208);
        SetStep(0x43);
        return;
    case 0x43:
        if (Counter(0) != 0) return;
        ChangeArea(0x8D, 0x5F8000, 0xA8000, 0);
        Area_EnterTransition = 0xE;
        AreaW1f_MusicCurrent = 0xFF;
        SetStep(0x44);
        return;
    case 0x44:
        if (!WaitClear()) return;
        if (EffectBusy(::bof3::Img(Scena14_Slot_addr))) return;
        SH_CALL(Music_LoadFile)(0x50);
        SetStep(0x45);
        BumpCounter0();
        return;
    case 0x45:
        if (Counter(0) != 3) return;
        if (Spawn(0x13, 0xFCBC, AngleYW(), 0x34A, 0x20) == 0) return;
        SetStep(0x46);
        return;
    case 0x46:
        if (EffectBusy(::bof3::Img(Scena14_Slot_addr))) return;
        Set(0x14);
        SetTimer(0x1E);
        SetStep(0x47);
        return;
    case 0x47:
        if (TimerRuns()) return;
        if (Spawn(0x13, 0xFD56, AngleYW(), 0x200, 0x20) == 0) return;
        SetStep(0x48);
        BumpCounter0();
        return;
    case 0x48:
        if (EffectBusy(::bof3::Img(Scena14_Slot_addr))) return;
        SetStep(0x49);
        BumpCounter0();
        return;
    case 0x49:
        if (Counter(0) != 0) return;
        Clear40();
        SH_CALL(Music_Play)(0x50, 8);
        StopRun();
        return;
    default:
        return;
    }
}

// original 0x567040: a line by who leads - for the first of the four bytes at
// `who` that any member's ObjTrio +0x89 (members below Field_MemberCount, read
// once) equals, Msg_OpenScript(lines[i]), Field_Request 2, and ax lines[i];
// none, ax 0xFFFF. Its callers do not read the answer.
SC13_EXPORT std::uint16_t __cdecl Scena14_TalkByMember(const unsigned char* who, const std::uint16_t* lines) {
    const unsigned char n = Field_MemberCount;
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned m = 0; m < n; ++m)
            if (B(::bof3::Img(scena_sc13::at::kLeaderName) + m * scena_sc13::at::kTrioStride) == who[i]) {
                Msg(lines[i]);
                SetRequest(2);
                return lines[i];
            }
    return 0xFFFF;
}

// original 0x5670E0: run 7, steps 0..7, 0xA..0x13, 0x14..0x20.
SC13_EXPORT void __cdecl Scena14_Run7(void) {
    switch (Step()) {
    case 0: Farewell(0x15, 1, 0); return;
    case 1: Farewell(0x16, 2, scena_sc13::at::kArea141a.v); return;
    case 2: Farewell(0x17, 3, scena_sc13::at::kArea141b.v); return;
    case 3: Farewell(0x19, 6, scena_sc13::at::kArea141d.v); return;
    case 4: Farewell(0x1A, 7, scena_sc13::at::kArea141e.v); return;
    case 5: Farewell(0x1C, 0, 0); return;
    case 6:
        if (Counter(0) != 7) return;
        SetPass(0);
        FadeOutStop(4);
        SetStep(7);
        SetTimer(8);
        return;
    case 7:
        if (TimerRuns()) return;
        BumpCounter0();
        StopRun();
        SetPass(0x1F);
        return;
    case 0xA:
        if (SH_CALL(File_LoadDone)() == 0) return;
        DropIn(3);
        SH_CALL(Music_Play)(0x9F, 8);
        SetStep(0xB);
        return;
    case 0xB:
        if (Counter(0) != 0x19) return;
        if (Request() == 2) return;
        SetPass(0);
        SetTimer(0xA);
        SetStep(0xC);
        return;
    case 0xC:
        if (TimerRuns()) return;
        SetPass(0x1F);
        BumpCounter0();
        SetStep(0xD);
        return;
    case 0xD:
        if (Counter(0) != 0) return;
        SH_AT(PlaceFn, scena_sc13::at::kPartyPlace.v)(0x2C8000, 0x278000, 0x25);
        SH_CALL(Field_StartEventBattle)(0x33);
        SetStep(0xE);
        return;
    case 0xF:
        ChangeArea(0xAC, 0x208000, 0x268000, 0x89);
        AreaW1f_MusicCurrent = 0xFF;
        SetStep(0x10);
        return;
    case 0x10:
        if (Counter(0) != 1) return;
        Spawn(0x31, AngleXW(), AngleYW(), AngleFBW(), 0x14);
        SH_CALL(Music_LoadFile)(0x9F);
        SetStep(0x11);
        return;
    case 0x11: {
        if (Counter(0) != 9) return;
        const unsigned char slot = SH_CALL(Effect_FindFree)();   // a slot of its own
        if (slot != 0xFF) FxAtCell(slot, 0x97);
        SetStep(0x12);
        return;
    }
    case 0x12:
        if (Counter(0) != 0xA) return;
        SetTimer(0x78);
        SetStep(0x13);
        return;
    case 0x14:
        if (!WaitClear()) return;
        BumpCounter0();
        SetPass(0);
        Say(0x22);
        SetStep(0x15);
        return;
    case 0x15:
        if (Request() == 2) return;
        CallA(0xB);
        SH_CALL(Task_Sleep)(1);
        SetRequest(6);
        SetStep(0x16);
        return;
    case 0x16:
        if (Request() != 0) return;
        SetRequest(1);
        ScenaSc6_Load10 = 1;
        SetStep(0x17);
        return;
    case 0x17: {
        if (Request() != 0) return;
        Draw_OtSlot = 4;
        Draw_SortOnX = 0;
        MapView_Redraw = 2;
        DropIn(0xA);
        SH_AT(PlaceFn, scena_sc13::at::kPartyPlace.v)(0x258000, 0x278000, 0x22);
        RestoreRecords();
        SetPass(0x1F);
        Transition(1);
        SetTimer(0x40);
        SetStep(0x18);
        const unsigned char slot = SH_CALL(Effect_FindFree)();   // a slot of its own
        if (slot != 0xFF) FxAtCell(slot, 0x98);
        return;
    }
    case 0x18:
        if (TimerRuns()) return;
        SH_CALL(Field_StartEventBattle)(0x34);
        SetStep(0x19);
        return;
    case 0x1A:
        BumpCounter0();
        Set(0x1F);
        ChangeArea(0xAC, 0x208000, 0x268000, 3);
        StopRun();
        return;
    case 0x1E:
        SH_AT(VoidFn, scena_sc13::at::kSound587B80.v)();
        SH_CALL(Sound_LoadStream)(2);
        SH_AT(ArgFn, scena_sc13::at::kParty591900.v)(0xC);
        Say(0x2A);
        SetStep(0x1F);
        return;
    case 0x1F:
        if (SH_CALL(Sound_StreamDone)() == 0) return;
        if (Request() == 2) return;
        SH_CALL(Sound_ResumeAll)();
        SetStep(0x20);
        BumpCounter0();
        return;
    case 0x20:
        if (Counter(0) != 0) return;
        SetBit80();
        Set(0x31);
        Clear40();
        StopRun();
        return;
    default:
        return;
    }
}

// original 0x5677B0: slot 1 - Scena14_Objects[object +0x86] (object, the flag
// bits' pointer).
SC13_EXPORT void __cdecl Scena14_ObjectTrigger(unsigned char* object) {
    const std::uint32_t bits = Chapter_FlagRow;
    const unsigned index = object[0x86];
    reinterpret_cast<ObjectEntry>(static_cast<std::uintptr_t>(
        Entry(::bof3::Img(Scena14_Objects_addr), static_cast<int>(index), scena_sc13::at::kObjectCount14, "Scena14_ObjectTrigger")))(object, bits);
}

// original 0x5677D0 / 0x567810 / 0x567850 / 0x567890 / 0x5678D0: entries 0..4,
// the line for 8, 4, 2, 5 and 6.
SC13_EXPORT void __cdecl Scena14_Object00(unsigned char* /*object*/, std::uint32_t /*bits*/) { AreaLine(8); }

SC13_EXPORT void __cdecl Scena14_Object01(unsigned char* /*object*/, std::uint32_t /*bits*/) { AreaLine(4); }

SC13_EXPORT void __cdecl Scena14_Object02(unsigned char* /*object*/, std::uint32_t /*bits*/) { AreaLine(2); }

SC13_EXPORT void __cdecl Scena14_Object03(unsigned char* /*object*/, std::uint32_t /*bits*/) { AreaLine(5); }

SC13_EXPORT void __cdecl Scena14_Object04(unsigned char* /*object*/, std::uint32_t /*bits*/) { AreaLine(6); }

// original 0x567910: entry 5 - ScriptFlags_Set40, run 5, step 5.
SC13_EXPORT void __cdecl Scena14_Object05(unsigned char* /*object*/, std::uint32_t /*bits*/) {
    Set40();
    SetRun(5);
    SetStep(5);
}

// original 0x567930: entry 6 - ScriptFlags_Set40, run 5, step 0x1E, the
// object's word +0x8A + 1.
SC13_EXPORT void __cdecl Scena14_Object06(unsigned char* object, std::uint32_t /*bits*/) {
    Set40();
    SetRun(5);
    SetStep(0x1E);
    std::uint16_t* const w = reinterpret_cast<std::uint16_t*>(object + 0x8A);
    *w = static_cast<std::uint16_t>(*w + 1);
}

// original 0x567950: entry 7 - ScriptFlags_Set40, run 7, step 0x1E, +0x8A + 1.
SC13_EXPORT void __cdecl Scena14_Object07(unsigned char* object, std::uint32_t /*bits*/) {
    Set40();
    SetRun(7);
    SetStep(0x1E);
    std::uint16_t* const w = reinterpret_cast<std::uint16_t*>(object + 0x8A);
    *w = static_cast<std::uint16_t>(*w + 1);
}

// original 0x567970: slot 2, the step hook. Area 0x94 at Cond_ByteFD 4 (any
// other value answers 0 at once) with flag 0x10 clear, x exactly 0x738000 and
// z 0x36..0x37: run 4 step 0, al 1. Area 0xBF with flag 5 set: the cell byte
// 0xA6 (AreaMap_ByteAt) under the step - the cell (x, z), and (x + 1, z) /
// (x, z + 1) / (x + 1, z + 1) where x's / z's fraction is not 0 - starts run 2
// at step 0x14, al 1. Otherwise al 0.
SC13_EXPORT unsigned char __cdecl Scena14_StepHook(int x, int z) {
    const std::uint32_t xw = static_cast<std::uint32_t>(x);
    const std::uint32_t zw = static_cast<std::uint32_t>(z);
    const unsigned x16 = xw >> 16, z16 = zw >> 16;
    if (Area() == 0x94) {
        if (FD() != 4) return 0;
        if (!Flag(0x10) && xw == 0x738000 && In(z16, 0x36, 2)) return Hit(4, 0);
    }
    if (Area() != 0xBF) return 0;
    if (!Flag(5)) return 0;
    const auto at = [](unsigned cx, unsigned cz) {
        return SH_CALL(AreaMap_ByteAt)(static_cast<short>(cx), static_cast<short>(cz)) == 0xA6;
    };
    unsigned char hits = at(x16, z16) ? 1 : 0;
    if ((xw & 0xFFFF) != 0 && at(x16 + 1, z16)) ++hits;
    if ((zw & 0xFFFF) != 0 && at(x16, z16 + 1)) ++hits;
    if ((xw & 0xFFFF) != 0 && (zw & 0xFFFF) != 0 && at(x16 + 1, z16 + 1)) ++hits;
    if (hits == 0) return 0;
    return Hit(2, 0x14);
}

// original 0x567A90: slot 3, the arrive hook. Starts runs by rectangles (x
// whole, signed, below a bound; z by its high word; area 0x8E's first by x's
// high word and z whole) in areas 0x8D, 0x8E, 0xAC - answering al 0 even
// then - and in 0x94 and 0xBF, answering al 1; Cond_ByteFD not 0 in 0x94 or
// 0xAC answers 0 at once.
SC13_EXPORT unsigned char __cdecl Scena14_ArriveHook(int x, int z) {
    const unsigned x16 = static_cast<std::uint32_t>(x) >> 16;
    const unsigned z16 = static_cast<std::uint32_t>(z) >> 16;
    const auto start = [](unsigned char run, unsigned char step) {
        Set40();
        SetRun(run);
        SetStep(step);
    };
    if (Area() == 0x8D) {
        if (FD() == 0) {
            if (!Flag(0x15)) {
                if (x < 0x4F8000 && In(z16, 7, 8)) start(7, 0);
            } else if (!Flag(0x16)) {
                if (x < 0x498000 && In(z16, 9, 4)) start(7, 1);
            } else if (!Flag(0x17)) {
                if (x < 0x408000 && In(z16, 9, 4)) start(7, 2);
            }
        }
        if (FD() == 1 && SH_CALL(Flags_Test)(Story(), 0x7A) != 0) {
            if (!Flag(0x19)) {
                if (x < 0x398000 && In(z16, 0x31, 4)) start(7, 3);
            } else if (!Flag(0x1A)) {
                if (x < 0x2D8000 && In(z16, 0x31, 4)) start(7, 4);
            }
        }
    }
    if (Area() == 0x8E) {
        if (FD() == 1 && !Flag(0x1C) && z < 0x1D8000 && In(x16, 0x13, 4)) start(7, 5);
        if (FD() == 3 && !Flag(0x1F) && x < 0x2C8000 && In(z16, 0x26, 4)) {
            Set40();
            FadeOutStop(0xA);
            SH_CALL(Music_LoadFile)(0x9F);
            SetRun(7);
            SetStep(0xA);
        }
    }
    if (Area() == 0x94) {
        if (FD() != 0) return 0;
        if (!Flag(0x11) && (z16 & 0xFFFF) == 0x1C && In(x16, 0x12, 3)) {
            start(4, 0xA);
            return 1;
        }
    }
    if (Area() == 0xAC) {
        if (FD() != 0) return 0;
        if (!Flag(0x13) && x < 0xB8000 && In(z16, 0x3F, 2)) start(6, 0xA);
    }
    if (Area() != 0xBF) return 0;
    if (FD() != 0) return 0;
    if (!In(x16, 0x1D, 5)) return 0;
    if (!In(z16, 0x14, 2)) return 0;
    if (!Flag(8)) return 0;
    if (Flag(9)) return 0;
    start(2, 0x1D);
    return 1;
}

void Scena142_Inject() {
    BOF3_INJECT(Scena14_Frame);
    BOF3_INJECT(ScenaShared_State0);
    BOF3_INJECT(Scena14_EnterArea);
    BOF3_INJECT(Scena14_GreyClut);
    BOF3_INJECT(Scena14_SpawnEffect);
    BOF3_INJECT(Scena14_Run);
    BOF3_INJECT(Scena14_Run1);
    BOF3_INJECT(Scena14_Run2);
    BOF3_INJECT(Scena14_Shake);
    BOF3_INJECT(Scena14_LeaveToC4);
    BOF3_INJECT(Scena14_Run3);
    BOF3_INJECT(Scena14_Run4);
    BOF3_INJECT(Scena14_ScrollView);
    BOF3_INJECT(Scena14_Run5);
    BOF3_INJECT(Scena14_Run6);
    BOF3_INJECT(Scena14_TalkByMember);
    BOF3_INJECT(Scena14_Run7);
    BOF3_INJECT(Scena14_ObjectTrigger);
    BOF3_INJECT(Scena14_Object00);
    BOF3_INJECT(Scena14_Object01);
    BOF3_INJECT(Scena14_Object02);
    BOF3_INJECT(Scena14_Object03);
    BOF3_INJECT(Scena14_Object04);
    BOF3_INJECT(Scena14_Object05);
    BOF3_INJECT(Scena14_Object06);
    BOF3_INJECT(Scena14_Object07);
    BOF3_INJECT(Scena14_StepHook);
    BOF3_INJECT(Scena14_ArriveHook);
}

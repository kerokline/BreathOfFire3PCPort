// Chapter 6's bank of scenario code, 0x54A910..0x54F080 (round ten group
// SC6). docs/scena_sc6.md.
//
//   - The vtable Scena06_Hooks 0x6610E0 (0x662C80's entry 6): slot 0
//     Scena06_Frame, a tail jump through Scena06_States on the state byte;
//     slot 1 Scena06_ObjectTrigger, a call through Scena06_Objects on the
//     object's +0x86; slot 2 Scena06_StepHook (x, z) answering in al; slot 3
//     the shared Scenario_NoHook; slot 4 Scena06_CellHook (x, z), the record
//     search 0x56D800 over Scena06_Cells and a tail jump through
//     Scena06_CellHandlers.
//   - State 0 Scena06_Start (character record 7 from Scena06_GuestStats),
//     state 1 Scena06_EnterArea (every way out stores state 2), state 2
//     Scena06_Run, a tail jump through Scena06_Runs on MoveScript_Var7; the
//     runs 1..17 are each a switch on the step byte 0x8034E5 whose cases wait
//     on a counter, the request byte or the wait word, do one thing and set
//     the next step.
//   - Scena06_Leap and its three phases (Scena06_LeapPhases): a jump arc of
//     Sprite_Current, called only by area 77's handler 0x40F090.
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. The tables
// are read in place and their entries called directly; the fuzz swaps them
// for recorders or typed stand-ins. No divergence: each function is a
// faithful replacement, except that a dispatcher whose index lies outside its
// table (a negative state or run, or one reading the next table) and a divide
// by a zero count abort where the original would jump through its neighbour
// or fault - the project's rule (round9 doc section 6).
#include "game/scena_sc6.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/scena_sc6_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = scena_sc6::at;
using scena_sc6::CellEntry;
using scena_sc6::CellFindFn;
using scena_sc6::ByteFn;
using scena_sc6::LeapEntry;
using scena_sc6::ObjectEntry;
using scena_sc6::PlaceFn;
using scena_sc6::VoidFn;
using scena_sc6::ZennyFn;

unsigned char& B(std::uint32_t a) { return *reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }
std::uint16_t& W(std::uint32_t a) { return *reinterpret_cast<std::uint16_t*>(static_cast<std::uintptr_t>(a)); }
std::uint32_t& D(std::uint32_t a) { return *reinterpret_cast<std::uint32_t*>(static_cast<std::uintptr_t>(a)); }
std::uint32_t Addr(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
std::uint16_t& W(unsigned char* p) { return W(Addr(p)); }
std::int32_t Get32(const unsigned char* p) {
    std::int32_t v;
    std::memcpy(&v, p, 4);
    return v;
}
void Put32(unsigned char* p, std::int32_t v) { std::memcpy(p, &v, 4); }

unsigned Area() { return W(at::kArea); }
unsigned char Counter(unsigned k) { return B(at::kCounters + k); }
void SetCounter(unsigned k, unsigned char v) { B(at::kCounters + k) = v; }
unsigned char Step() { return B(at::kStep); }
void SetStep(unsigned char v) { B(at::kStep) = v; }
void SetRun(unsigned char v) { B(at::kRun) = v; }
void SetState(unsigned char v) { B(at::kState) = v; }
void SetPass(unsigned char v) { B(at::kPassFlags) = v; }
unsigned char Request() { return B(at::kRequest); }
void SetRequest(unsigned char v) { B(at::kRequest) = v; }
bool Waiting() { return W(at::kWait) != 0; }
bool Busy() { return Request() == 2; }   // a message still open
void SetMusicByte(unsigned char v) { B(at::kMusicByte) = v; }
void ScriptOr8() { B(at::kScriptFlags) = static_cast<unsigned char>(B(at::kScriptFlags) | 8); }       // or byte
void ScriptXor8() { W(at::kScriptFlags) = static_cast<std::uint16_t>(W(at::kScriptFlags) ^ 8); }     // xor word
void StatusOr1() { B(at::kStatusBits) = static_cast<unsigned char>(B(at::kStatusBits) | 1); }
void StatusXor1() { B(at::kStatusBits) = static_cast<unsigned char>(B(at::kStatusBits) ^ 1); }

// The four counters 0x903848..B zeroed, in that order; or the last three.
void ClearCounters() {
    for (unsigned k = 0; k < 4; ++k) SetCounter(k, 0);
}
void ClearCounters123() {
    for (unsigned k = 1; k < 4; ++k) SetCounter(k, 0);
}

// The flag bits are read from 0x929ED0 afresh for every call, as the
// originals load the dword before each push.
unsigned char* Bits() { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(D(at::kFlagBits))); }
unsigned char* Story() { return &B(at::kStoryFlags); }
bool Flag(unsigned i) { return SH_CALL(Flags_Test)(Bits(), i) != 0; }   // test al, al
bool StoryFlag(unsigned i) { return SH_CALL(Flags_Test)(Story(), i) != 0; }
void Set(unsigned i) { SH_CALL(Flags_Set)(Bits(), i); }
void Clr(unsigned i) { SH_CALL(Flags_Clear)(Bits(), i); }
void SetStory(unsigned i) { SH_CALL(Flags_Set)(Story(), i); }
void ClrStory(unsigned i) { SH_CALL(Flags_Clear)(Story(), i); }

void Set40() { SH_CALL(ScriptFlags_Set40)(); }
void Clear40() { SH_CALL(ScriptFlags_Clear40)(); }
void Msg(unsigned short id) { SH_CALL(Msg_OpenScript)(id); }
// The chapter's message: Msg_OpenScript(id), the request byte 2.
void Say(unsigned short id) {
    Msg(id);
    SetRequest(2);
}
void ChangeArea(unsigned area, int x, int z, unsigned flags) { SH_CALL(Field_ChangeArea)(area, x, z, flags); }
void CallA(unsigned n) { SH_CALL(Scenario_CallA)(n); }
void CallB(unsigned n) { SH_CALL(Scenario_CallB)(n); }
void DropIn(unsigned entry) { SH_CALL(Party_DropIn)(entry); }   // the answer is not read
void Kind2(unsigned char a) { SH_CALL(Kind2_Place)(a); }
void Transition(unsigned char kind) { SH_CALL(Transition_Start)(kind); }
void MusicPlay(unsigned track) { SH_CALL(Music_Play)(track, 8); }
void MusicStop(int frames) { SH_CALL(Music_FadeOutStop)(frames); }
void Sound(unsigned short id) { SH_CALL(Sound_PlayEffect)(id); }
void Battle(unsigned id) { SH_CALL(Field_StartEventBattle)(id); }
void PartyPass() { SH_AT(VoidFn, at::kPartyPass)(); }
void PartyPlace(int x, int z, unsigned kind) { SH_AT(PlaceFn, at::kPartyPlace)(x, z, kind); }
void ZennyAdd(unsigned amount) { SH_AT(ZennyFn, at::kZennyAdd)(amount, 0); }

// A run's end: ScriptFlags_Clear40, then the step and the run 0.
void EndRun() {
    Clear40();
    SetStep(0);
    SetRun(0);
}
// ... with the counters 1..3 zeroed between (counter 0 stays).
void EndRun123() {
    Clear40();
    ClearCounters123();
    SetStep(0);
    SetRun(0);
}
// ... with all four.
void EndRunAll() {
    Clear40();
    ClearCounters();
    SetStep(0);
    SetRun(0);
}

// The character record of MoveScript_EffectState[k] (the member's), its +0xB.
unsigned char& RecordB(unsigned k) {
    const unsigned member = B(at::kEffectState + k);
    return B(at::kCharRecords + at::kCharStride * member + 0xB);
}

unsigned char* Effect(unsigned slot) { return &B(at::kEffects + (slot & 0xFFu) * at::kEffectStride); }
std::int32_t Angle(std::uint32_t a) { return static_cast<std::int16_t>(W(a)); }

// Effect_FindFree into the slot byte 0x903850 (stored before it is tested);
// false for none (0xFF).
bool TakeSlot() {
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    B(at::kEffectSlot) = slot;
    return slot != 0xFF;
}
// The slot byte's record: live, a kind, at (x, y, z), a life at +9.
void Place(unsigned char kind, std::int32_t x, std::int32_t y, std::int32_t z, unsigned char life) {
    unsigned char* const e = Effect(B(at::kEffectSlot));
    e[0] = 1;
    e[5] = kind;
    Put32(e + 0x64, x);
    Put32(e + 0x68, y);
    Put32(e + 0x6C, z);
    e[9] = life;
}
// Effect_FindFree kept in a local (not the slot byte): live and a kind.
void Spawn(unsigned char kind) {
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    unsigned char* const e = Effect(slot);
    e[0] = 1;
    e[5] = kind;
}

// For each of the three party bytes 0x904062.. equal to a member, that
// member's line: the messages first for `a`, then `b`, then `c`, each byte
// read afresh.
void MemberLines(unsigned char a, unsigned short ma, unsigned char b, unsigned short mb, unsigned char c,
                 unsigned short mc) {
    for (unsigned i = 0; i < 3; ++i)
        if (B(at::kPartyBytes + i) == a) Msg(ma);
    for (unsigned i = 0; i < 3; ++i)
        if (B(at::kPartyBytes + i) == b) Msg(mb);
    for (unsigned i = 0; i < 3; ++i)
        if (B(at::kPartyBytes + i) == c) Msg(mc);
}

// An effect of kind 6 for a party member's field object: Sprite_Current set to
// `object`, Effect_FindFree into the object's +0xB (Sprite_Current read back
// after the call); none (0xFF), nothing. Else the record live, kind 6, +6 =
// `six`, +0xC 0, +0x10 the s8 Scena06_MemberBytes[the byte `member`, read
// after the call], +0x2E / +0x30 the object's words.
void MemberEffect(std::uint32_t object, unsigned char six, std::uint32_t member) {
    D(at::kSpriteCurrent) = object;
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    unsigned char* const s = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(D(at::kSpriteCurrent)));
    s[0xB] = slot;
    if (s[0xB] == 0xFF) return;
    Effect(s[0xB])[0] = 1;
    Effect(s[0xB])[5] = 6;
    Effect(s[0xB])[6] = six;
    Put32(Effect(s[0xB]) + 0xC, 0);
    Put32(Effect(s[0xB]) + 0x10, static_cast<signed char>(B(at::kMemberBytes + B(member))));
    W(Effect(s[0xB]) + 0x2E) = W(s + 0x2E);
    W(Effect(s[0xB]) + 0x30) = W(s + 0x30);
}

// A table entry read in place, the index checked against the table.
std::uint32_t Entry(std::uint32_t table, int index, unsigned count, const char* who) {
    if (index < 0 || static_cast<unsigned>(index) >= count)
        bof3::Fatal("%s: index %d outside its table of %u at 0x%X", who, index, count, (unsigned)table);
    return D(table + 4 * static_cast<std::uint32_t>(index));
}

unsigned char* Sprite() { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(D(at::kSpriteCurrent))); }

}  // namespace

// Exported with C linkage (the symbols.gen.h prototypes); no tail calls, so a
// Fatal's stack shows the dispatcher.
#define SC6_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// Slot 0 and the states

// original 0x54A910: slot 0, Field_ModeDispatch's call every field frame - a
// tail jump through Scena06_States on the s8 state 0x8034E2: 0 Scena06_Start,
// 1 Scena06_EnterArea, 2 Scena06_Run.
SC6_EXPORT void __cdecl Scena06_Frame(void) {
    const int state = static_cast<signed char>(B(at::kState));
    reinterpret_cast<VoidFn>(static_cast<std::uintptr_t>(Entry(at::kStates, state, at::kStateCount, "Scena06_Frame")))();
}

// original 0x54A920: state 0 - Scena06_GuestRecord, then state 1.
SC6_EXPORT void __cdecl Scena06_Start(void) {
    SH_CALL(Scena06_GuestRecord)();
    SetState(1);
}

// original 0x54E700: character record 7 (0x903EEC) from the eight bytes of
// Scena06_GuestStats: HP +0x18 = byte 0; the equipment bytes +0x12 = byte 6,
// +0x15 = byte 7; the words max HP +0x20 / +0x40 = byte 1, ATK +0x24 / +0x44
// = byte 2, DEF +0x26 / +0x46 = byte 3, AGI +0x28 / +0x48 = byte 4, INT +0x2A
// / +0x4A = byte 5 (each zero-extended; the field names are
// docs/char-stats.md section 2's).
SC6_EXPORT void __cdecl Scena06_GuestRecord(void) {
    const unsigned char* const t = &B(at::kGuestStats);
    unsigned char* const r = &B(at::kRecord7);
    W(r + 0x18) = t[0];
    r[0x12] = t[6];
    r[0x15] = t[7];
    W(r + 0x20) = t[1];
    W(r + 0x40) = t[1];
    W(r + 0x24) = t[2];
    W(r + 0x44) = t[2];
    W(r + 0x26) = t[3];
    W(r + 0x46) = t[3];
    W(r + 0x28) = t[4];
    W(r + 0x48) = t[4];
    W(r + 0x2A) = t[5];
    W(r + 0x4A) = t[5];
}

// original 0x54E3B0: the call-table entries by the selector Cond_Flags
// +0x19C & 0x7F: 0 B3 A1, 2 B4 A1, 3 B3 A2, 4 B5 A3, 6 B4 A2; 1, 5 and any
// other value none.
SC6_EXPORT void __cdecl Scena06_PartyCalls(void) {
    switch (B(at::kSelector) & 0x7F) {
    case 0: CallB(3); CallA(1); return;
    case 2: CallB(4); CallA(1); return;
    case 3: CallB(3); CallA(2); return;
    case 4: CallB(5); CallA(3); return;
    case 6: CallB(4); CallA(2); return;
    default: return;
    }
}

// original 0x54A930: state 1, the area just entered. Each area test reads
// Game_AreaNumber afresh; every way out stores state 2.
//   0x27, flag 3 clear: counters 0 and 1 0, and by the selector 0 / 1 / 2 two
//     members' records +0xB bit 0 cleared and call B 0 / 1 / 2;
//   0x2E, flag 0xB set and 0x3C clear: the pass flags 0, ScriptFlags_Set40,
//     counters 0, step 0, run 0x11, flag 0x3C, then by the selector 1..6
//     calls B and A (5 none);
//   0x35: flag 0xB clear - call A 0 and counters 0; flag 0xE set and 0x11
//     clear - the pass flags 0x1F and Scena06_PartyCalls;
//   0x4C, flag 0x2C set and 0x3F clear: counters 0, flag 0x3F, 0x56D6F0;
//   0x57, counter 2 at 1 (flag 0x1F clear: pass flags 0x1F) or 2 (flag 0x21
//     clear: flag 0x21): ScriptFlags_Clear40, counters, step and run 0; any
//     other counter 2: counters 0 and state 2 at once;
//   0x5C, counter 2: 1 (flag 0x1A clear: the pass flags 0); 2 the pass flags
//     0x1F and state 2 at once; 4 0x532ED0(0x1D0000, 0x3E0000, 0x1B); any
//     other: state 2 at once;
//   0x5E, counter 2: 1 (flag 0xF clear: the pass flags 0x1F); 2 (flag 0x13
//     clear: counter 3 0, Camera_Distance 0x480, a redraw, the pass flags 0,
//     Scena06_PartyCalls); then, flag 0x11 set and 0x3B clear: flag 0x3B,
//     the pass flags 0, Scena06_PartyCalls, step 0x1E, run 6;
//   then areas 0x2D, 0x41, 0x57, 0x10 clear the counters.
SC6_EXPORT void __cdecl Scena06_EnterArea(void) {
    if (Area() == 0x27 && !Flag(3)) {
        const unsigned selector = B(at::kSelector) & 0x7F;
        SetCounter(0, 0);
        SetCounter(1, 0);
        switch (selector) {
        case 0:
            RecordB(5) &= 0xFE;
            RecordB(6) &= 0xFE;
            CallB(0);
            break;
        case 1:
            RecordB(6) &= 0xFE;
            RecordB(2) &= 0xFE;
            CallB(1);
            break;
        case 2:
            RecordB(2) &= 0xFE;
            RecordB(5) &= 0xFE;
            CallB(2);
            break;
        default: break;
        }
    }
    if (Area() == 0x2E && Flag(0xB) && !Flag(0x3C)) {
        SetPass(0);
        Set40();
        unsigned char* const bits = Bits();
        ClearCounters();
        SetStep(0);
        SetRun(0x11);
        SH_CALL(Flags_Set)(bits, 0x3C);
        switch (B(at::kSelector) & 0x7F) {
        case 1: CallB(6); CallA(7); break;
        case 2: CallB(4); CallA(7); break;
        case 3: CallB(6); CallA(8); break;
        case 4: CallB(4); CallA(8); break;
        case 6: CallB(7); CallA(9); break;
        default: break;   // 5 and anything outside 1..6
        }
    }
    if (Area() == 0x35) {
        if (!Flag(0xB)) {
            CallA(0);
            ClearCounters();
        }
        if (Flag(0xE) && !Flag(0x11)) {
            SetPass(0x1F);
            SH_CALL(Scena06_PartyCalls)();
        }
    }
    if (Area() == 0x4C && Flag(0x2C) && !Flag(0x3F)) {
        unsigned char* const bits = Bits();
        ClearCounters();
        SH_CALL(Flags_Set)(bits, 0x3F);
        SH_AT(VoidFn, at::kSetBit80)();
    }
    if (Area() == 0x57) {
        const unsigned c2 = Counter(2);
        bool clear = false;
        if (c2 == 1) {
            if (!Flag(0x1F)) {
                SetPass(0x1F);
                clear = true;
            }
        } else if (c2 == 2) {
            if (!Flag(0x21)) {
                Set(0x21);
                clear = true;
            }
        } else {
            ClearCounters();
            SetState(2);
            return;
        }
        if (clear) {
            Clear40();
            ClearCounters();
            SetStep(0);
            SetRun(0);
        }
    }
    if (Area() == 0x5C) {
        switch (Counter(2)) {
        case 1:
            if (!Flag(0x1A)) SetPass(0);
            break;
        case 2:
            SetPass(0x1F);
            SetState(2);
            return;
        case 4: PartyPlace(0x1D0000, 0x3E0000, 0x1B); break;
        default: SetState(2); return;
        }
    }
    if (Area() == 0x5E) {
        const unsigned c2 = Counter(2);
        if (c2 == 1) {
            if (!Flag(0xF)) SetPass(0x1F);
        } else if (c2 == 2) {
            if (!Flag(0x13)) {
                SetCounter(3, 0);
                W(at::kCameraDistance) = 0x480;
                B(at::kRedraw) = 2;
                SetPass(0);
                SH_CALL(Scena06_PartyCalls)();
            }
        }
        if (Flag(0x11) && !Flag(0x3B)) {
            Set(0x3B);
            SetPass(0);
            SH_CALL(Scena06_PartyCalls)();
            SetStep(0x1E);
            SetRun(6);
        }
    }
    const unsigned area = Area();
    if (area == 0x2D || area == 0x41 || area == 0x57 || area == 0x10) ClearCounters();
    SetState(2);
}

// original 0x54ADD0: state 2, a tail jump through Scena06_Runs on the s8 run
// MoveScript_Var7: 0 a bare ret (0x437CC0), 1..17 Scena06_Run01..17.
SC6_EXPORT void __cdecl Scena06_Run(void) {
    const int run = static_cast<signed char>(B(at::kRun));
    reinterpret_cast<VoidFn>(static_cast<std::uintptr_t>(Entry(at::kRuns, run, at::kRunCount, "Scena06_Run")))();
}

// ===========================================================================
// The runs (Scena06_Runs 1..17), each a switch on the step 0x8034E5; a step
// the switch does not hold does nothing.

// original 0x54ADE0: run 1, steps 0..4, 7, 0xA.
SC6_EXPORT void __cdecl Scena06_Run01(void) {
    switch (Step()) {
    case 0:
        if (Busy()) return;
        SetCounter(0, 1);
        DropIn(0);
        SetStep(1);
        return;
    case 1: {
        if (Counter(0) != 8) return;
        unsigned char* const bits = Bits();
        SetStep(2);
        SH_CALL(Flags_Set)(bits, 0x3E);
        ChangeArea(0x27, 0x1E0000, 0x270000, 0x81);
        return;
    }
    case 2:
        Kind2(1);
        Set(3);
        SetStep(3);
        return;
    case 3:
        if (Counter(0) != 0x11) return;
        SetCounter(0, 0);
        SetStep(4);
        W(at::kTimer) = 0xB4;
        SetPass(0);
        ChangeArea(0x27, 0x598000, 0x100000, 0x82);
        return;
    case 4: {
        // the timer counted down every frame (to 0xFFFF past 0); at 0 on entry
        const std::uint16_t t = W(at::kTimer);
        W(at::kTimer) = static_cast<std::uint16_t>(t - 1);
        if (t != 0) return;
        Transition(5);
        SetPass(0x1F);
        SetStep(7);
        return;
    }
    case 7:
        if (Waiting()) return;
        SetCounter(0, 1);
        SetStep(0xA);
        return;
    case 10:
        if (Counter(0) != 2) return;
        if (B(at::kHold) != 0) return;
        Clr(0x3E);
        Set(3);
        Clear40();
        SetCounter(0, 0);
        SetStep(0);
        SetRun(0);
        return;
    default: return;
    }
}

namespace {
// The runs' common close: flag `f`, ScriptFlags_Clear40, counter 0, the step
// and the run 0.
void SetAndEnd0(unsigned f) {
    Set(f);
    Clear40();
    SetCounter(0, 0);
    SetStep(0);
    SetRun(0);
}
void End0() {
    Clear40();
    SetCounter(0, 0);
    SetStep(0);
    SetRun(0);
}
}  // namespace

// original 0x54AF90: run 2, steps 0, 1, 5, 6, 0xA, 0xB, 0xF..0x13.
SC6_EXPORT void __cdecl Scena06_Run02(void) {
    switch (Step()) {
    case 0:
        if (Busy()) return;
        SetCounter(0, 1);
        DropIn(3);
        SetStep(1);
        return;
    case 1:
        if (Counter(0) != 4) return;
        SetAndEnd0(4);
        return;
    case 5:
        Say(0x42);
        SetStep(6);
        return;
    case 6:
        if (Busy()) return;
        End0();
        return;
    case 10: {
        if (Busy()) return;
        const unsigned lead = B(at::kObjTrio + 8);
        SetCounter(0, 0xF);
        if (lead == 2) DropIn(4);
        else if (lead == 3) DropIn(5);
        else if (lead == 4) DropIn(6);
        else return;
        SetStep(0xB);
        return;
    }
    case 11:
        if (Counter(0) != 0x14) return;
        SetAndEnd0(5);
        return;
    case 15:
        if (Busy()) return;
        SetCounter(0, 0x19);
        DropIn(8);
        SetStep(0x10);
        return;
    case 16:
        if (Counter(0) != 0x1A) return;
        if (!TakeSlot()) return;
        SetCounter(1, B(at::kEffectSlot));
        SetStep(0x11);
        Place(0x13, Angle(at::kAngle0), Angle(at::kAngle1), 0x3C, 0x3C);
        return;
    case 17:
        if (Effect(Counter(1))[0] != 0) return;
        SetCounter(0, 0x1B);
        SetCounter(1, 0);
        SetStep(0x12);
        return;
    case 18:
        if (Counter(0) != 0x1C) return;
        if (!TakeSlot()) return;
        SetStep(0x13);
        Place(0x13, Angle(at::kAngle0), Angle(at::kAngle1), 0x200, 0x3C);
        return;
    case 19:
        if (Counter(0) != 0x1E) return;
        SetAndEnd0(7);
        return;
    default: return;
    }
}

// original 0x54B250: run 3, steps 0, 1, 3..5, 7..0xF.
SC6_EXPORT void __cdecl Scena06_Run03(void) {
    switch (Step()) {
    case 0:
        SetStep(1);
        ChangeArea(0x2F, 0x718000, 0x90000, 0x80);
        SetMusicByte(0xFF);
        return;
    case 1:
        if (Waiting()) return;
        SetCounter(0, 1);
        Kind2(0);
        SetStep(3);
        return;
    case 3: {
        if (Counter(0) != 3) return;
        if (!TakeSlot()) return;
        SetStep(4);
        Place(0x31, Angle(at::kAngle0), Angle(at::kAngle1), 0x124, 0x5A);
        Put32(Effect(B(at::kEffectSlot)) + 0xC, 0x700);
        return;
    }
    case 4:
        if (Counter(0) != 7) return;
        MusicPlay(0x27);
        ScriptOr8();
        SetStep(5);
        return;
    case 5:
        if (Counter(0) != 0xA) return;
        if (!TakeSlot()) return;
        SetStep(7);
        Place(0x13, -0x3B5, Angle(at::kAngle1), Angle(at::kAngle2), 0x1E);
        return;
    case 7:
        MusicStop(0xA);
        Sound(0x202);
        SetStep(8);
        return;
    case 8:
        if (Counter(0) != 0xC) return;
        MusicPlay(0x24);
        Sound(0x203);
        ScriptXor8();
        SetStep(9);
        return;
    case 9:
        if (Counter(0) != 0xD) return;
        if (!TakeSlot()) return;
        SetStep(0xA);
        Place(0x13, -0x2AA, Angle(at::kAngle1), Angle(at::kAngle2), 0x1E);
        return;
    case 10: {
        if (Counter(0) != 0xE) return;
        unsigned char* const bits = Bits();
        ScriptOr8();
        SetStep(0xB);
        SH_CALL(Flags_Set)(bits, 0xA);
        ChangeArea(0x35, 0x230000, 0x290000, 0x80);
        return;
    }
    case 11:
        Kind2(0);
        if (!TakeSlot()) return;
        SetStep(0xC);
        Place(0x13, 0, Angle(at::kAngle1), Angle(at::kAngle2), 2);
        return;
    case 12:
        if (Counter(0) != 1) return;
        if (!TakeSlot()) return;
        SetStep(0xD);
        Place(0x13, -0x2AA, Angle(at::kAngle1), Angle(at::kAngle2), 0x78);
        return;
    case 13:
        if (Counter(0) != 2) return;
        SetStep(0xE);
        Set(0xB);
        Set(9);
        SH_AT(ByteFn, at::kKeyItemPut)(4);
        return;
    case 14: {
        if (Counter(0) != 0x32) return;
        const unsigned member6 = B(at::kEffectState + 6);
        ScriptXor8();
        SetStep(0xF);
        B(at::kCharRecords + at::kCharStride * member6 + 0xB) |= 1;
        RecordB(2) |= 1;
        ChangeArea(0x2D, 0x220000, 0x2A0000, 3);
        return;
    }
    case 15:
        PartyPass();
        EndRunAll();
        return;
    default: return;
    }
}

// original 0x54B6C0: run 4, steps 0..7, 0xA..0xC, 0x14..0x16, 0x1E, 0x1F.
SC6_EXPORT void __cdecl Scena06_Run04(void) {
    switch (Step()) {
    case 0:
        Say(0xD);
        SetStep(1);
        return;
    case 1:
        if (Busy()) return;
        DropIn(0);
        SetStep(2);
        return;
    case 2:
        if (Counter(0) != 1) return;
        D(at::kKind2X) = 0x350000;
        D(at::kKind2Z) = D(at::kLead38);
        W(at::kF3Divisor) = 0x20;
        SetStep(3);
        return;
    case 3:
        if (B(at::kHold) != 0) return;
        SetCounter(0, 2);
        SetStep(4);
        return;
    case 4:
        if (Counter(0) != 0xA) return;
        Transition(0xD);
        SetStep(5);
        return;
    case 5:
        if (Waiting()) return;
        SetPass(0);
        Say(0x1A);
        SetStep(6);
        return;
    case 6: {
        if (Busy()) return;
        unsigned char* const bits = Bits();
        SetCounter(2, 1);
        SetStep(7);
        SH_CALL(Flags_Set)(bits, 0xD);
        ChangeArea(0x5E, 0x330000, 0x210000, 0x81);
        return;
    }
    case 7:
        if (Counter(0) != 0xD) return;
        Set(0xF);
        EndRun123();
        return;
    case 10:
        Say(0x29);
        SetStep(0xB);
        return;
    case 11:
        if (Busy()) return;
        DropIn(2);
        SetStep(0xC);
        return;
    case 12:
        if (Counter(0) != 0x15) return;
        Set(0xE);
        EndRun123();
        return;
    case 20:
        Say(0x29);
        SetStep(0x15);
        return;
    case 21:
        if (Busy()) return;
        DropIn(0);
        SetStep(0x16);
        return;
    case 22:
        if (Counter(0) != 1) return;
        Set(0x10);
        EndRun123();
        return;
    case 30:
        Say(0x13);
        SetStep(0x1F);
        return;
    case 31:
        if (Busy()) return;
        EndRun();
        return;
    default: return;
    }
}

// original 0x54B9C0: run 5, steps 0..5.
SC6_EXPORT void __cdecl Scena06_Run05(void) {
    switch (Step()) {
    case 0:
        Transition(0xD);
        SetStep(1);
        return;
    case 1:
        if (Waiting()) return;
        SetPass(0);
        SetStep(2);
        ChangeArea(0x35, 0x120000, 0x180000, 0x81);
        return;
    case 2: {
        if (Counter(0) != 0xC) return;
        Transition(0xD);
        PartyPass();
        unsigned char* const bits = Bits();
        SetStep(3);
        SH_CALL(Flags_Set)(bits, 0x11);
        MusicStop(0x1E);
        return;
    }
    case 3:
        if (Waiting()) return;
        SetPass(0);
        SH_CALL(Sound_LoadStream)(0);
        SetStep(4);
        return;
    case 4:
        if (SH_CALL(Sound_StreamDone)() == 0) return;
        SetStep(5);
        ChangeArea(0x57, 0x310000, 0x2E0000, 3);
        return;
    case 5:
        SetPass(0x1F);
        EndRun123();
        return;
    default: return;
    }
}

// original 0x54BAE0: run 6, steps 0..0xA, 0x14..0x18, 0x1E..0x20.
// As the original has it: step 6 moves Camera_Distance by -0x40 a frame for
// 0x12 frames, counted in counter 3.
SC6_EXPORT void __cdecl Scena06_Run06(void) {
    switch (Step()) {
    case 0:
        DropIn(3);
        SetStep(1);
        return;
    case 1: {
        if (Counter(0) != 9) return;
        unsigned char* const bits = Bits();
        SetStep(2);
        SetCounter(2, 2);
        SH_CALL(Flags_Set)(bits, 0x12);
        ChangeArea(0x5E, 0x2E0000, 0x2E0000, 0x84);
        return;
    }
    case 2:
        Say(0x38);
        SetStep(3);
        return;
    case 3:
        if (Busy()) return;
        Transition(5);
        SetPass(0x1F);
        SetStep(4);
        return;
    case 4:
        if (Waiting()) return;
        SetCounter(0, 1);
        SetStep(5);
        return;
    case 5:
        if (Counter(0) != 2) return;
        if (!TakeSlot()) return;
        SetCounter(0, 3);
        SetStep(6);
        Place(0x13, -0x210, Angle(at::kAngle1), 0x2F2, 0x14);
        return;
    case 6: {
        const unsigned char n = static_cast<unsigned char>(Counter(3) + 1);
        W(at::kCameraDistance) = static_cast<std::uint16_t>(W(at::kCameraDistance) - 0x40);
        B(at::kRedraw) = 2;
        SetCounter(3, n);
        if (n < 0x12) return;
        B(at::kRedraw) = 2;
        W(at::kCameraDistance) = 0;
        SetCounter(3, 0);
        SetStep(7);
        return;
    }
    case 7:
        if (Counter(0) != 0x13) return;
        Kind2(0);
        if (!TakeSlot()) return;
        SetCounter(3, B(at::kEffectSlot));
        SetStep(8);
        Place(0x13, -0x2AA, Angle(at::kAngle1), 0x200, 0x1E);
        return;
    case 8:
        if (Effect(Counter(3))[0] != 0) return;
        SetCounter(0, 0x14);
        SetStep(9);
        return;
    case 9: {
        if (Counter(0) != 0x16) return;
        unsigned char* const bits = Bits();
        SetStep(0xA);
        SH_CALL(Flags_Set)(bits, 0x13);
        ChangeArea(0x5E, 0x2C0000, 0x290000, 0x85);
        SetMusicByte(0x42);
        return;
    }
    case 10:
        if (Counter(0) != 0x22) return;
        Set(0x10);
        EndRun123();
        return;
    case 20:
        DropIn(6);
        Kind2(1);
        SetStep(0x15);
        return;
    case 21:
        if (Counter(0) != 4) return;
        D(at::kKind2X) = D(at::kLead34);
        D(at::kKind2Z) = D(at::kLead38);
        W(at::kF3Divisor) = 0x20;
        SetStep(0x16);
        return;
    case 22:
        if (B(at::kHold) != 0) return;
        SetCounter(0, 5);
        SetStep(0x17);
        return;
    case 23:
        if (Counter(0) != 7) return;
        ZennyAdd(0x3E8);
        SetCounter(0, 8);
        SetStep(0x18);
        return;
    case 24:
        if (Counter(0) != 9) return;
        Set(0x14);
        EndRun123();
        return;
    case 30:
        SetStep(0x1F);
        ChangeArea(0x5E, 0x520000, 0x330000, 0x8E);
        return;
    case 31:
        SetPass(0x1F);
        SetStep(0x20);
        return;
    case 32:
        if (Counter(0) != 0x36) return;
        EndRun();
        return;
    default: return;
    }
}

namespace {
// Run 7's area change back into 0x5C with the request 7 already answered:
// counter 2 2, flags 0x1A and 0x1B, area 0x5C at (0x190000, 0x3C0000) 0x81,
// the music byte 0x3C.
void Run07Back() {
    Set(0x1A);
    Set(0x1B);
    ChangeArea(0x5C, 0x190000, 0x3C0000, 0x81);
    SetMusicByte(0x3C);
}
// Run 7's leaving 0x5C: 0x533E50, Field_StatusBits' bit 0 toggled, flags
// 0x15 and 0x17 cleared, area 0x57 at (0x310000, 0x2D0000) 4.
void Run07Leave() {
    PartyPass();
    StatusXor1();
    Clr(0x15);
    Clr(0x17);
    ChangeArea(0x57, 0x310000, 0x2D0000, 4);
}
// Character record 7's two equipment bytes both set: `yes`, else `no`.
unsigned char GuestEquipped(unsigned char yes, unsigned char no) {
    return B(at::kRecord7 + 0x12) != 0 && B(at::kRecord7 + 0x15) != 0 ? yes : no;
}
// The request 7 with the byte 0x904C9F `v`, the pass flags 0, a step.
void Request7(unsigned char v, bool pass, unsigned char step) {
    SetRequest(7);
    B(at::kByte904C9F) = v;
    if (pass) SetPass(0);
    SetStep(step);
}
}  // namespace

// original 0x54BF20: run 7, steps 0..6, 0xA, 0xB, 0xE, 0xF, 0x12..0x15,
// 0x19..0x21, 0x23..0x25, 0x28, 0x2D, 0x32, 0x33, 0x3C..0x3E.
SC6_EXPORT void __cdecl Scena06_Run07(void) {
    switch (Step()) {
    case 0x00:
        if (Busy()) return;
        SetStep(1);
        return;
    case 0x01: {
        if (Busy()) return;
        const unsigned char bits = static_cast<unsigned char>(B(at::kStatusBits) | 1);
        SetCounter(2, 1);
        SetStep(2);
        B(at::kStatusBits) = bits;
        ChangeArea(0x5C, 0xE0000, 0x3C0000, 0x80);
        SetMusicByte(0xFF);
        return;
    }
    case 0x02:
        Say(1);
        SetStep(3);
        return;
    case 0x03:
        if (Busy()) return;
        SetPass(0x1F);
        Kind2(0);
        SetStep(4);
        return;
    case 0x04:
        if (Counter(0) != 2) return;
        Request7(0, true, 5);
        return;
    case 0x05:
        if (Request() != 0) return;
        SetStep(6);
        return;
    case 0x06: SetStep(GuestEquipped(0xA, 0xE)); return;
    case 0x0A:
        SetCounter(2, 2);
        SetStep(0xB);
        Run07Back();
        return;
    case 0x0B:
    case 0x0F:
    case 0x21:
    case 0x2D:
    case 0x33:
        if (Counter(0) != 3) return;
        EndRun123();
        return;
    case 0x0E: {
        unsigned char* const bits = Bits();
        SetCounter(2, 2);
        SetStep(0xF);
        SH_CALL(Flags_Set)(bits, 0x17);
        ChangeArea(0x5C, 0x190000, 0x3C0000, 0x82);
        return;
    }
    case 0x12: {
        if (Busy()) return;
        unsigned char* const bits = Bits();
        SetCounter(0, 1);
        SH_CALL(Flags_Set)(bits, 0x1B);
        SetStep(0x13);
        return;
    }
    case 0x13:
        if (Counter(0) != 2) return;
        ZennyAdd(0x3E8);
        SetCounter(0, 3);
        SetStep(0x14);
        return;
    case 0x14:
        if (Counter(0) != 4) return;
        SetStep(0x15);
        Run07Leave();
        return;
    case 0x15:
    case 0x25:
        Clear40();
        SetCounter(0, 0);
        ClearCounters123();
        SetStep(0);
        SetRun(0);
        return;
    case 0x19:
        if (Busy()) return;
        SetStep(0x1A);
        return;
    case 0x1A: {
        if (Busy()) return;
        const unsigned char bits = static_cast<unsigned char>(B(at::kStatusBits) | 1);
        SetPass(0);
        SetStep(0x1B);
        B(at::kStatusBits) = bits;
        ChangeArea(0x5C, 0xE0000, 0x3C0000, 0x80);
        return;
    }
    case 0x1B:
        Say(1);
        SetStep(0x1C);
        return;
    case 0x1C:
        if (Busy()) return;
        SetPass(0x1F);
        Kind2(0);
        SetStep(0x1D);
        return;
    case 0x1D:
        if (Counter(0) != 2) return;
        Request7(0, true, 0x1E);
        return;
    case 0x1E:
        if (Request() != 0) return;
        SetStep(0x1F);
        return;
    case 0x1F: SetStep(GuestEquipped(0x28, 0x20)); return;
    case 0x20: {
        unsigned char* const bits = Bits();
        SetCounter(2, 2);
        SetStep(0x21);
        SH_CALL(Flags_Set)(bits, 0x17);
        ChangeArea(0x5C, 0x190000, 0x3C0000, 0x82);
        return;
    }
    case 0x23:
        if (Busy()) return;
        SetCounter(0, 1);
        SetStep(0x24);
        DropIn(3);
        return;
    case 0x24:
        if (Counter(0) != 3) return;
        SetStep(0x25);
        Run07Leave();
        return;
    case 0x28:
        SetStep(0x2D);
        SetCounter(2, 2);
        Run07Back();
        return;
    case 0x32: {
        if (Busy()) return;
        const unsigned char bits = static_cast<unsigned char>(B(at::kStatusBits) | 1);
        SetStep(0x33);
        SetCounter(2, 1);
        B(at::kStatusBits) = bits;
        ChangeArea(0x5C, 0x190000, 0x3C0000, 0x88);
        return;
    }
    case 0x3C:
        if (Busy()) return;
        Request7(0, true, 0x3D);
        return;
    case 0x3D:
        if (Request() != 0) return;
        SetStep(0x3E);
        return;
    case 0x3E: SetStep(GuestEquipped(0x28, 0x20)); return;
    default: return;
    }
}

namespace {
// Run 8's step 0: the three party field objects sorted by their +0x89
// (ascending, unsigned; an exchange sort of the whole 0x14C-byte records),
// the second party list's Field_MemberCount bytes copied to 0x939A10, that
// list reset to (0, 0xFF, 0xFF), the old count to 0x939A02, the count 1.
void SortParty() {
    unsigned char* const trio = &B(at::kObjTrio);
    for (unsigned i = 1; i <= 2; ++i) {
        unsigned char* const a = trio + at::kObjStride * (i - 1);
        for (unsigned j = i; j < 3; ++j) {
            unsigned char* const b = trio + at::kObjStride * j;
            if (a[0x89] > b[0x89]) {
                unsigned char tmp[at::kObjStride];
                std::memcpy(tmp, a, at::kObjStride);
                std::memcpy(a, b, at::kObjStride);
                std::memcpy(b, tmp, at::kObjStride);
            }
        }
    }
    const unsigned char count = B(at::kMemberCount);
    for (unsigned i = 0; i < count; ++i) B(at::kTempList + i) = B(at::kPartyList2 + i);
    B(at::kPartyList2) = 0;
    B(at::kPartyList2 + 1) = 0xFF;
    B(at::kPartyList2 + 2) = 0xFF;
    B(at::kTempCount) = count;
    B(at::kMemberCount) = 1;
}
}  // namespace

// original 0x54C480: run 8, steps 0..2, 4, 5, 0xA..0x12, 0x14..0x16,
// 0x1E..0x20, 0x28, 0x29.
SC6_EXPORT void __cdecl Scena06_Run08(void) {
    switch (Step()) {
    case 0x00:
        PartyPass();
        Kind2(1);
        SortParty();
        SetStep(1);
        return;
    case 0x01:
        PartyPlace(0x190000, 0x410000, 0x19);
        SetStep(2);
        return;
    case 0x02:
        Battle(0x19);
        SetStep(3);
        return;
    case 0x04:
        PartyPass();
        SetCounter(2, 2);
        SetStep(5);
        ChangeArea(0x5C, 0x190000, 0x3C0000, 0x85);
        return;
    case 0x05:
        if (Counter(0) != 4) return;
        EndRun123();
        return;
    case 0x0A:
        if (Busy()) return;
        DropIn(6);
        Kind2(2);
        SetStep(0xB);
        return;
    case 0x0B:
        if (Counter(0) != 2) return;
        PartyPlace(0x190000, 0x410000, 0x1A);
        SetStep(0xC);
        return;
    case 0x0C:
        if (Counter(0) != 3) return;
        Battle(0x1A);
        SetStep(0xD);
        return;
    case 0x0D:
        if (Counter(0) != 4) return;
        SetCounter(2, 2);
        SetStep(0xE);
        ChangeArea(0x5C, 0x190000, 0x3C0000, 0x87);
        return;
    case 0x0E:
        if (Counter(0) != 0xA) return;
        Request7(1, false, 0xF);
        return;
    case 0x0F:
        if (Request() != 0) return;
        SetStep(0x10);
        return;
    case 0x10:
        Say(0x23);
        SetStep(0x11);
        return;
    case 0x11: {
        if (Busy()) return;
        SetStep(0x12);
        PartyPass();
        const unsigned char status = static_cast<unsigned char>(B(at::kStatusBits) ^ 1);
        unsigned char* const bits = Bits();
        B(at::kStatusBits) = status;
        SH_CALL(Flags_Set)(bits, 0x1C);
        ChangeArea(0x57, 0x310000, 0x2D0000, 4);
        return;
    }
    case 0x12:
    case 0x16:
    case 0x20:
    case 0x29:
        EndRunAll();
        return;
    case 0x14:
        if (Busy()) return;
        SetRequest(6);
        SetStep(0x15);
        return;
    case 0x15:
        if (Request() != 0) return;
        SetStep(0x16);
        return;
    case 0x1E:
        if (Busy()) return;
        Request7(0, false, 0x1F);
        return;
    case 0x1F:
        if (Request() != 0) return;
        SetStep(0x20);
        return;
    case 0x28:
        if (Busy()) return;
        SetStep(0x29);
        PartyPass();
        StatusXor1();
        ChangeArea(0x57, 0x310000, 0x2D0000, 4);
        return;
    default: return;
    }
}

namespace {
// Run 9's steps 1 and 3: flag 0x1D, Field_StatusBits' bit 0 set, counter 2 4,
// area 0x5C at (0x180000, 0x3D0000) 0x89, the music byte 0x3C.
void Run09Enter() {
    unsigned char* const bits = Bits();
    SetStep(5);
    SH_CALL(Flags_Set)(bits, 0x1D);
    StatusOr1();
    SetCounter(2, 4);
    ChangeArea(0x5C, 0x180000, 0x3D0000, 0x89);
    SetMusicByte(0x3C);
}
// Run 9's steps 0x12 and 0x17: 0x533E50, counter 2 1, step 6, flag 0x1D
// cleared, Field_StatusBits' bit 0 toggled, area 0x57 at (0x310000,
// 0x2D0000) 4.
void Run09Leave() {
    PartyPass();
    unsigned char* const bits = Bits();
    SetCounter(2, 1);
    SetStep(6);
    SH_CALL(Flags_Clear)(bits, 0x1D);
    StatusXor1();
    ChangeArea(0x57, 0x310000, 0x2D0000, 4);
}
}  // namespace

// original 0x54C910: run 9, steps 0..3, 5, 7, 0xA, 0xF..0x12, 0x14..0x17.
SC6_EXPORT void __cdecl Scena06_Run09(void) {
    switch (Step()) {
    case 0x00:
        if (Busy()) return;
        SetCounter(0, 1);
        SetStep(1);
        return;
    case 0x01:
    case 0x03:
        if (Counter(0) != 0xA) return;
        Run09Enter();
        return;
    case 0x02:
        if (Busy()) return;
        SetCounter(0, 1);
        SetStep(3);
        return;
    case 0x05:
        if (Counter(0) != 0xF) return;
        Battle(0x1B);
        SetStep(6);
        return;
    case 0x07: {
        DropIn(0xE);
        unsigned char* const bits = Bits();
        SetStep(0xA);
        SH_CALL(Flags_Set)(bits, 0x1F);
        ChangeArea(0x5C, 0x180000, 0x3C0000, 0x8A);
        SetMusicByte(0xFF);
        return;
    }
    case 0x0A: {
        if (Counter(0) != 0x14) return;
        PartyPass();
        const unsigned char status = static_cast<unsigned char>(B(at::kStatusBits) ^ 1);
        SetCounter(2, 2);
        SetStep(6);
        B(at::kStatusBits) = status;
        ChangeArea(0x57, 0x310000, 0x2D0000, 4);
        return;
    }
    case 0x0F:
        DropIn(0xB);
        SetStep(0x10);
        return;
    case 0x10:
        if (Counter(0) != 0x14) return;
        Transition(0xD);
        Set(0x20);
        SetStep(0x11);
        return;
    case 0x15:
        if (Counter(0) != 2) return;
        Transition(0xD);
        Set(0x20);
        SetStep(0x16);
        return;
    case 0x11:
        if (Waiting()) return;
        SetPass(0);
        Say(0x2B);
        SetStep(0x12);
        return;
    case 0x16:
        if (Waiting()) return;
        SetPass(0);
        Say(0x29);
        SetStep(0x17);
        return;
    case 0x12:
    case 0x17:
        if (Busy()) return;
        Run09Leave();
        return;
    case 0x14:
        DropIn(0xD);
        SetStep(0x15);
        return;
    default: return;
    }
}

// original 0x54CC80: run 10, steps 0..3, 5, 6.
SC6_EXPORT void __cdecl Scena06_Run10(void) {
    switch (Step()) {
    case 0:
        if (Busy()) return;
        SetCounter(0, 1);
        DropIn(7);
        SetStep(1);
        return;
    case 1:
        if (Counter(0) != 8) return;
        SH_AT(VoidFn, at::kSoundJmp)();
        SH_CALL(Sound_LoadStream)(2);
        SetStep(2);
        return;
    case 2:
        if (SH_CALL(Sound_StreamDone)() == 0) return;
        SH_CALL(Sound_ResumeAll)();
        SetCounter(0, 9);
        SetStep(3);
        return;
    case 3:
        if (Counter(0) != 0xA) return;
        EndRun();
        return;
    case 5:
        DropIn(8);
        Set(0x24);
        SetStep(6);
        return;
    case 6:
        if (Counter(0) != 0x17) return;
        EndRun123();
        return;
    default: return;
    }
}

// original 0x54CD80: run 11, steps 0, 1, 5..9. Step 0 sets step 1 before its
// flag test; step 6 ends the run unless counter 0 is 1.
SC6_EXPORT void __cdecl Scena06_Run11(void) {
    switch (Step()) {
    case 0: {
        unsigned char* const bits = Bits();
        SetStep(1);
        Say(SH_CALL(Flags_Test)(bits, 0x27) != 0 ? 0xA : 8);
        return;
    }
    case 1:
        if (Busy()) return;
        EndRunAll();
        return;
    case 5:
        if (Flag(0x27)) {
            Say(0xA);
            SetStep(1);
        } else if (Flag(0x25)) {
            Say(8);
            SetStep(1);
        } else {
            Say(9);
            SetStep(6);
        }
        return;
    case 6:
        if (Busy()) return;
        if (Counter(0) != 1) {
            EndRunAll();
            return;
        }
        SetStep(7);
        return;
    case 7:
        DropIn(0);
        SetStep(8);
        return;
    case 8:
        if (Counter(0) != 2) return;
        SetStep(9);
        return;
    case 9:
        if (Counter(0) != 3) return;
        Set(0x25);
        EndRun123();
        return;
    default: return;
    }
}

// original 0x54E2F0: an effect of kind 6 for the lead's field object (ObjTrio
// record 0; Sprite_Current is set to it): its +6 the low byte of `arg` +
// 0x70, +0x10 Scena06_MemberBytes[the first party byte 0x904062]
// (MemberEffect above). Runs 12 and 14 pass 0x91 and 0x93 (+6 = 1 and 3).
SC6_EXPORT void __cdecl Scena06_LeaderEffect(unsigned arg) {
    MemberEffect(at::kObjTrio, static_cast<unsigned char>(arg + 0x70), at::kPartyBytes);
}

// original 0x54CF00: run 12, steps 0..8, 0xA..0x13.
SC6_EXPORT void __cdecl Scena06_Run12(void) {
    switch (Step()) {
    case 0x00:
        SH_CALL(Scena06_LeaderEffect)(0x91);
        SetStep(1);
        return;
    case 0x01:
        DropIn(0);
        Kind2(0);
        SetStep(2);
        return;
    case 0x02:
        if (Counter(0) != 1) return;
        MusicStop(0xA);
        MusicPlay(0x45);
        SetStep(3);
        return;
    case 0x03:
        if (Counter(0) != 3) return;
        PartyPlace(0x1D0000, 0x1D0000, 0x17);
        SetStep(4);
        return;
    case 0x04:
        if (Counter(0) != 5) return;
        Battle(0x17);
        SetStep(5);
        return;
    case 0x05:
        if (Counter(0) != 0xA) return;
        DropIn(1);
        SetStep(6);
        return;
    case 0x06:
        if (Counter(0) != 0x1D) return;
        Transition(4);
        SetStep(7);
        return;
    case 0x07:
        if (Waiting()) return;
        SetPass(0);
        SetCounter(0, 0x1F);
        MusicStop(0xA);
        MusicPlay(0x3A);
        Say(0xD);
        SetStep(8);
        return;
    case 0x08:
        if ((B(at::kBank7DEE44) & 2) == 0) return;
        SH_CALL(Party_AddToLists)(2);
        SetRequest(6);
        B(at::kLoad0F) = 0;
        SetStep(0xA);
        return;
    case 0x0A:
        if (B(at::kLoad0F) == 0) return;
        SetStep(0xB);
        return;
    case 0x0B:
        SetStep(0xC);
        RecordB(2) &= 0xFD;
        SetRequest(1);
        B(at::kLoad10) = 1;
        return;
    case 0x0C:
        if (B(at::kLoad10) == 0) {
            SetPass(0x1F);
            PartyPlace(0x1D0000, 0x1D0000, 0x18);
            MusicPlay(0x45);
            SetStep(0xD);
        }
        [[fallthrough]];
    case 0x0D:
        if (Request() != 0) return;
        SetCounter(0, 0x28);
        SetStep(0xE);
        return;
    case 0x0E:
        if (Counter(0) != 0x3C) return;
        Battle(0x18);
        SetStep(0xF);
        return;
    case 0x0F: {
        if (Counter(0) != 5) return;
        DropIn(3);
        Set(0);
        SetStep(0x10);
        Transition(0xD);
        return;
    }
    case 0x10:
        if (Waiting()) return;
        SetPass(0);
        SetStep(0x11);
        return;
    case 0x11: {
        unsigned char* const bits = Bits();
        SetStep(0x12);
        SH_CALL(Flags_Set)(bits, 1);
        switch (B(at::kSelector) & 0x7F) {
        case 0:
            SetCounter(2, 0);
            ChangeArea(0x43, 0x1D0000, 0x1C0000, 0x82);
            return;
        case 3:
            SetCounter(2, 0);
            CallB(6);
            break;
        case 4:
            SetCounter(2, 0);
            CallB(4);
            break;
        default: return;
        }
        CallA(6);
        ChangeArea(0x43, 0x1D0000, 0x1C0000, 0x82);
        return;
    }
    case 0x12:
        SetPass(0x1F);
        SetStep(0x13);
        return;
    case 0x13:
        if (Counter(0) != 0x14) return;
        MusicPlay(0x44);
        ClearCounters123();
        Clear40();
        SetStep(0);
        SetRun(0);
        return;
    default: return;
    }
}

// original 0x54D2B0: run 13, steps 0..8. Step 1's effect slot is kept in a
// local (the slot byte 0x903850 is not written); step 3 goes to 4 with flag
// 0x26 set, else 6.
SC6_EXPORT void __cdecl Scena06_Run13(void) {
    switch (Step()) {
    case 0:
        Say(0xC);
        SetStep(1);
        return;
    case 1:
        if (Busy()) return;
        SH_CALL(AreaMap_SetByte)(0x5D, 0x2E, 0);
        Spawn(0x45);
        Clr(0x26);
        Set(0x2D);
        SetStep(2);
        return;
    case 2:
        if (Flag(0x2D)) return;
        SetStep(3);
        return;
    case 3: SetStep(Flag(0x26) ? 4 : 6); return;
    case 4:
        SH_CALL(AreaMap_SetByte)(0x5D, 0x2E, 0x51);
        SetStory(0x2F);
        Sound(0x201);
        DropIn(1);
        SetStep(5);
        return;
    case 5:
        if (Counter(0) != 1) return;
        Set(0x27);
        Sound(0x204);
        MusicPlay(0x5B);
        ClearCounters123();
        Clear40();
        SetStep(0);
        SetRun(0);
        return;
    case 6:
        SH_CALL(AreaMap_SetByte)(0x5D, 0x2E, 0x51);
        if (!Flag(0x25)) {
            SetStep(7);
            return;
        }
        SetStory(0x2F);
        Sound(0x201);
        Sound(0x204);
        MusicPlay(0x5B);
        DropIn(2);
        Clr(0x25);
        SetStep(8);
        return;
    case 7:
        ClrStory(0x2F);
        SetCounter(0, 0);
        ClearCounters123();
        Clear40();
        SetStep(0);
        SetRun(0);
        return;
    case 8:
        if (Counter(0) != 1) return;
        ClrStory(0x2F);
        ClearCounters123();
        Clear40();
        SetStep(0);
        SetRun(0);
        return;
    default: return;
    }
}

// original 0x54DA20: run 14's step 0xB - each party byte 0x904062.. equal to
// 2, then 1, then 5, its line (script messages 0x16, 0x15, 0x14); the
// request 2, step 0xC.
SC6_EXPORT void __cdecl Scena06_MemberLines(void) {
    MemberLines(2, 0x16, 1, 0x15, 5, 0x14);
    SetRequest(2);
    SetStep(0xC);
}

namespace {
// Run 14's and 15's close: flag `f` (the flag bits read before), then the
// counters 1..3, ScriptFlags_Clear40, the step and the run 0.
void SetAndEnd123(unsigned char* bits, unsigned f) {
    SH_CALL(Flags_Set)(bits, f);
    ClearCounters123();
    Clear40();
    SetStep(0);
    SetRun(0);
}
}  // namespace

// original 0x54D4F0: run 14, steps 0..2, 5, 0xA..0xD, 0x14..0x17, 0x1E,
// 0x23. Step 2 gives each member whose party byte is 0 an effect of kind 6
// (MemberEffect: ObjTrio records 0..2, +6 = 3).
SC6_EXPORT void __cdecl Scena06_Run14(void) {
    switch (Step()) {
    case 0x00:
        DropIn(3);
        Kind2(0);
        SetStep(1);
        return;
    case 0x01:
        if (Counter(0) != 4) return;
        MemberLines(2, 0x12, 5, 0x11, 1, 0x10);
        SetRequest(2);
        SetStep(2);
        return;
    case 0x02:
        if (Busy()) return;
        for (unsigned k = 0; k < 3; ++k)
            if (B(at::kPartyBytes + k) == 0) MemberEffect(at::kObjTrio + at::kObjStride * k, 3, at::kPartyBytes + k);
        SetStep(5);
        SetCounter(0, 5);
        return;
    case 0x05:
        if (Counter(0) != 0xA) return;
        SetAndEnd123(Bits(), 0x28);
        return;
    case 0x0A:
        if (Busy()) return;
        SH_CALL(Scena06_LeaderEffect)(0x93);
        SetStep(0xB);
        return;
    case 0x0B: SH_CALL(Scena06_MemberLines)(); return;
    case 0x0C:
        if (Busy()) return;
        DropIn(4);
        SetStep(0xD);
        return;
    case 0x0D:
        if (Counter(0) != 0xF) return;
        SetAndEnd123(Bits(), 0x29);
        return;
    case 0x14:
        ScriptOr8();
        MusicStop(0xA);
        DropIn(5);
        SetStep(0x15);
        return;
    case 0x15:
        if (Counter(0) != 0x15) return;
        Kind2(1);
        SetStep(0x16);
        return;
    case 0x16:
        if (Counter(0) != 0x1A) return;
        PartyPlace(0x2E0000, 0x60000, 0x1C);
        SetStep(0x17);
        return;
    case 0x17:
        if (Counter(0) != 0x1C) return;
        MemberLines(5, 0x19, 1, 0x18, 2, 0x17);
        SetRequest(2);
        SetStep(0x1E);
        return;
    case 0x1E:
        if (Busy()) return;
        Battle(0x1C);
        SetStep(0x23);
        return;
    case 0x23: {
        if (Counter(0) != 0x1E) return;
        unsigned char* const bits = Bits();
        ScriptXor8();
        SetAndEnd123(bits, 0x2A);
        return;
    }
    default: return;
    }
}

// original 0x54DA90: run 15, steps 0, 1, 5, 6, 9..0xE, 0x11..0x16, 0x1E.
SC6_EXPORT void __cdecl Scena06_Run15(void) {
    switch (Step()) {
    case 0x00:
        Say(0x1A);
        SetStep(1);
        return;
    case 0x01:
        if (Busy()) return;
        SetCounter(0, 0);
        ClearCounters123();
        Clear40();
        SetStep(0);
        SetRun(0);
        return;
    case 0x05:
        Say(0x1B);
        SetStep(6);
        return;
    case 0x06:
        if (Busy()) return;
        Spawn(0x4B);
        DropIn(6);
        B(at::kCondFE) = 1;
        SetStory(7);
        Sound(0x207);
        Sound(0x201);
        SetStep(9);
        return;
    case 0x09:
        if (Counter(0) != 0x29) return;
        Transition(0xF);
        SetStep(0xA);
        return;
    case 0x0A:
        if (Waiting()) return;
        Transition(0x10);
        DropIn(7);
        Kind2(2);
        SetStep(0xB);
        return;
    case 0x0B:
        if (Waiting()) return;
        SetCounter(0, 0x2A);
        SetStep(0xC);
        return;
    case 0x0C:
        if (Counter(0) != 0x2D) return;
        if (!TakeSlot()) return;
        SetCounter(3, B(at::kEffectSlot));
        SetStep(0xD);
        Place(0x13, -0x32E, Angle(at::kAngle1), Angle(at::kAngle2), 0x50);
        return;
    case 0x0D:
        if (Effect(Counter(3))[0] != 0) return;
        if (!TakeSlot()) return;
        SetCounter(3, B(at::kEffectSlot));
        SetStep(0xE);
        Place(0x13, -0x2AA, Angle(at::kAngle1), Angle(at::kAngle2), 0x28);
        return;
    case 0x0E:
        if (Effect(Counter(3))[0] != 0) return;
        SetCounter(3, 0);
        SetCounter(0, 0x2F);
        SetStep(0x11);
        return;
    case 0x11:
        if (Counter(0) != 0x35) return;
        MusicStop(0xA);
        MusicPlay(0x5C);
        SetStep(0x12);
        return;
    case 0x12:
        if (Counter(0) != 0x42) return;
        MusicStop(0x1E);
        SH_CALL(Sound_LoadStream)(2);
        SetStep(0x13);
        return;
    case 0x13:
        if (SH_CALL(Sound_StreamDone)() == 0) return;
        MusicPlay(0x5C);
        SetCounter(0, 0x43);
        SetStep(0x14);
        return;
    case 0x14:
        if (Counter(0) != 0x47) return;
        MemberLines(2, 0x25, 5, 0x24, 1, 0x23);
        SetRequest(2);
        SetStep(0x15);
        return;
    case 0x15:
        if (Busy()) return;
        SetCounter(0, 0x48);
        SetStep(0x16);
        return;
    case 0x16:
        if (Counter(0) != 0x49) return;
        Set(0x2C);
        SH_CALL(Inventory_Add)(0, 0x57, 1);   // a fourth word 0 pushed, unread
        ClearCounters123();
        Clear40();
        SetStep(0);
        SetRun(0);
        return;
    case 0x1E:
        Say(0x26);
        SetStep(1);
        return;
    default: return;
    }
}

namespace {
// Run 16's gift: Inventory_Add(category, character record 7's byte `at` + 1,
// 1); none added (al 0): counter 0 `none`, then counter 1, the step and the
// run cleared with ScriptFlags_Clear40. Else the item's name (Item_NamePtr,
// the byte read afresh) copied into the 16 bytes at `text`, script message
// `msg`, the request 2, flag `flag`, Sound_PlayEffect(0x106), step 0x15.
void Gift(unsigned category, std::uint32_t at_byte, unsigned char none, std::uint32_t text, unsigned short msg,
          unsigned flag) {
    const unsigned char id = static_cast<unsigned char>(B(at_byte) + 1);
    if (SH_CALL(Inventory_Add)(
            category, id, 1) == 0) {   // a fourth word 0 pushed, unread
        SetCounter(0, none);
        SetCounter(1, 0);
        Clear40();
        SetStep(0);
        SetRun(0);
        return;
    }
    const unsigned char again = static_cast<unsigned char>(B(at_byte) + 1);
    const unsigned char* const name = SH_CALL(Item_NamePtr)(category, again);
    for (unsigned i = 0; i < 16; i += 4) Put32(&B(text + i), Get32(name + i));
    Msg(msg);
    SetRequest(2);
    Set(flag);
    Sound(0x106);
    SetStep(0x15);
}
}  // namespace

// original 0x54DED0: run 16, steps 0, 1, 0xA, 0xC, 0x14, 0x15: character
// record 7's weapon (+0x12) and armour (+0x15) bytes + 1 given as items
// (Gift above: messages 0x94 / 0x95, flags 0x22 / 0x23, the names to
// Text_Records and Text_Records + 0x20).
SC6_EXPORT void __cdecl Scena06_Run16(void) {
    switch (Step()) {
    case 0x00:
        if (Busy()) return;
        SetCounter(0, 0x1E);
        SetStep(1);
        return;
    case 0x01:
        if (Counter(0) != 0x1F) return;
        Gift(1, at::kRecord7 + 0x12, 0x20, at::kTextRecords, 0x94, 0x22);
        return;
    case 0x0A:
        if (Busy()) return;
        SetCounter(0, 0x23);
        SetStep(0xC);
        return;
    case 0x0C:
        if (Counter(0) != 0x24) return;
        Gift(2, at::kRecord7 + 0x15, 0x25, at::kTextRecords + 0x20, 0x95, 0x23);
        return;
    case 0x14:
        SH_CALL(Msg_OpenSystem)(1);
        SetRequest(2);
        SetStep(0x15);
        return;
    case 0x15:
        if (Busy()) return;
        SetCounter(0, 0);
        SetCounter(1, 0);
        Clear40();
        SetStep(0);
        SetRun(0);
        return;
    default: return;
    }
}

// original 0x54E0C0: run 17, steps 0..8, 0xA.
SC6_EXPORT void __cdecl Scena06_Run17(void) {
    switch (Step()) {
    case 0:
        SetStep(1);
        ChangeArea(0x2E, 0xF8000, 0x2B0000, 0x80);
        return;
    case 1:
        Transition(1);
        SetPass(0x1F);
        SetStep(2);
        return;
    case 2:
        if (Waiting()) return;
        SetCounter(0, 3);
        SetStep(3);
        return;
    case 3:
        if (Counter(0) != 0xF) return;
        ScriptOr8();
        SetStep(4);
        return;
    case 4:
        if (Counter(0) != 0x14) return;
        if (!TakeSlot()) return;
        SetStep(5);
        Place(0x13, -0x2AA, Angle(at::kAngle1), 0xF8, 0x1E);
        return;
    case 5:
        if (Counter(0) != 0x18) return;
        if (!TakeSlot()) return;
        SetStep(6);
        Place(0x13, -0x2AA, Angle(at::kAngle1), 0x200, 0x1E);
        return;
    case 6:
        if (Counter(0) != 0x1B) return;
        SetStep(7);
        ChangeArea(0x3C, 0x2F0000, 0x530000, 0x80);
        return;
    case 7:
        ScriptXor8();
        SetStep(8);
        return;
    case 8: {
        if (Counter(0) != 9) return;
        unsigned char* const bits = Bits();
        SetStep(0xA);
        SH_CALL(Flags_Set)(bits, 0x3D);
        ChangeArea(0x57, 0x220000, 0x390000, 1);
        return;
    }
    case 10:
        EndRunAll();
        return;
    default: return;
    }
}

// ===========================================================================
// Slot 1, the object trigger, and Scena06_Objects' handlers

// original 0x54E440: slot 1, called by 0x56D6D0 with the object that
// triggered: Scena06_Objects[object +0x86] (object, the flag bits' pointer).
// 0x56D6D0 does not read the answer.
SC6_EXPORT void __cdecl Scena06_ObjectTrigger(unsigned char* object) {
    const std::uint32_t bits = D(at::kFlagBits);
    const unsigned index = object[0x86];
    reinterpret_cast<ObjectEntry>(static_cast<std::uintptr_t>(
        Entry(at::kObjects, static_cast<int>(index), at::kObjectCount, "Scena06_ObjectTrigger")))(object, bits);
}

namespace {
// The handlers' shape: ScriptFlags_Set40, the counters 0 (counter 0 `c0`), a
// step and a run - stored after the call, no call between.
void StartRun(unsigned char step, unsigned char run, unsigned char c0 = 0) {
    Set40();
    SetCounter(0, c0);
    ClearCounters123();
    SetStep(step);
    SetRun(run);
}
}  // namespace

// original 0x54E460 .. 0x54E6C0: Scena06_Objects entries 1..13 (entry 0 is
// the bare ret 0x437CC0). Each is called with (object, bits) and reads
// neither.
SC6_EXPORT void __cdecl Scena06_Object01(void) { StartRun(0, 1); }
SC6_EXPORT void __cdecl Scena06_Object02(void) { StartRun(0, 2); }
// original 0x54E4C0: once (flag 5): run 2 from step 0xA.
SC6_EXPORT void __cdecl Scena06_Object03(void) {
    if (Flag(5)) return;
    StartRun(0xA, 2);
}
// original 0x54E500: run 2 from step 0xF, then flag 4 (the bits read after
// ScriptFlags_Set40).
SC6_EXPORT void __cdecl Scena06_Object04(void) {
    Set40();
    ClearCounters();
    unsigned char* const bits = Bits();
    SetStep(0xF);
    SetRun(2);
    SH_CALL(Flags_Set)(bits, 4);
}
// original 0x54E540: flag 0xC only.
SC6_EXPORT void __cdecl Scena06_Object05(void) { Set(0xC); }
SC6_EXPORT void __cdecl Scena06_Object06(void) { StartRun(0, 4); }
SC6_EXPORT void __cdecl Scena06_Object07(void) { StartRun(0x14, 4); }
// original 0x54E5C0: run 6 from step 0 with counter 0 at 1.
SC6_EXPORT void __cdecl Scena06_Object08(void) { StartRun(0, 6, 1); }
SC6_EXPORT void __cdecl Scena06_Object09(void) { StartRun(0x14, 6); }
SC6_EXPORT void __cdecl Scena06_Object10(void) { StartRun(0, 0xA); }
SC6_EXPORT void __cdecl Scena06_Object11(void) { StartRun(0xA, 0xE); }
// original 0x54E680: run 0x10 from step 0x14 with flag 0x22 set, else 0;
// counters 0 and 1 zeroed before the test, the step and run after it.
SC6_EXPORT void __cdecl Scena06_Object12(void) {
    Set40();
    unsigned char* const bits = Bits();
    SetCounter(0, 0);
    SetCounter(1, 0);
    const bool set = SH_CALL(Flags_Test)(bits, 0x22) != 0;
    SetStep(set ? 0x14 : 0);
    SetRun(0x10);
}
// original 0x54E6C0: the same on flag 0x23, from step 0x14 or 0xA.
SC6_EXPORT void __cdecl Scena06_Object13(void) {
    Set40();
    SetCounter(0, 0);
    SetCounter(1, 0);
    const bool set = Flag(0x23);
    SetStep(set ? 0x14 : 0xA);
    SetRun(0x10);
}

// ===========================================================================
// Scena06_Leap: a jump arc of Sprite_Current, stepped once a frame by area
// 77's handler 0x40F090 (the one caller: E8 at 0x40F0E6) with (the
// movement-script object, dx and dz as s8, the lift and the gravity as s16,
// an animation (0xFF none) and a flag byte). Its phase is Sprite_Current +4.

// original 0x54E790: Scena06_LeapPhases[Sprite_Current +4] with the seven
// arguments, whose al it answers (area 77 steps its script on 0).
SC6_EXPORT unsigned char __cdecl Scena06_Leap(unsigned char* object, unsigned dx, unsigned dz, unsigned lift,
                                              unsigned gravity, unsigned animation, unsigned flag) {
    const unsigned phase = Sprite()[4];
    return reinterpret_cast<LeapEntry>(static_cast<std::uintptr_t>(
        Entry(at::kLeapPhases, static_cast<int>(phase), at::kLeapPhaseCount, "Scena06_Leap")))(
        object, dx, dz, lift, gravity, animation, flag);
}

// original 0x54E7D0: phase 0. The object's speed Field_MoveSpeeds[object +4]
// (unchecked); 0: al 0 and nothing. Else the sprite's +9 0; the steps the
// larger of |dx| and |dz| (s8, as a byte), +0xB = steps - 1 (0 for none);
// +0xA = 16 / speed frames a step; +0x14 = lift << 8; +0xC / +0x10 = dx / dz
// << 15 over frames x steps (0 for no steps); phase 1, al 1.
// The original divides by frames x steps with a speed above 16 (0 frames): a
// fault; ours aborts (docs/scena_sc6.md section 6).
SC6_EXPORT unsigned char __cdecl Scena06_LeapStart(unsigned char* object, unsigned dx, unsigned dz, unsigned lift,
                                                   unsigned, unsigned, unsigned) {
    const unsigned char speed = B(at::kMoveSpeeds + object[4]);
    if (speed == 0) return 0;
    unsigned char* const s = Sprite();
    const int x = static_cast<signed char>(dx);
    const int z = static_cast<signed char>(dz);
    s[9] = 0;
    const int ax = x < 0 ? -x : x;
    const int az = z < 0 ? -z : z;
    const unsigned char steps = static_cast<unsigned char>(ax < az ? az : ax);
    s[0xB] = steps != 0 ? static_cast<unsigned char>(steps - 1) : 0;
    s[0xA] = static_cast<unsigned char>(16 / static_cast<int>(speed));
    Put32(s + 0x14, static_cast<std::int32_t>(static_cast<std::int16_t>(lift)) * 0x100);
    if (steps != 0) {
        const int span = static_cast<int>(s[0xA]) * steps;
        if (span == 0)
            bof3::Fatal("Scena06_LeapStart: speed %u (Field_MoveSpeeds[%u]) gives 0 frames a step: the original divides by 0",
                        (unsigned)speed, (unsigned)object[4]);
        Put32(s + 0xC, x * 0x8000 / span);
        Put32(s + 0x10, z * 0x8000 / span);
        s[4] = 1;
        return 1;
    }
    Put32(s + 0x10, 0);
    Put32(s + 0xC, 0);
    s[4] = 1;
    return 1;
}

// original 0x54E8E0: phase 1, a frame in the air. x += +0xC, z += +0x10, the
// height +0x3E += +0x14 >> 8, +0x14 += gravity, the frame count +0xA - 1;
// the ground AreaMap_Elevation(x, z): falling (+0x14 negative) below it
// (ground above +0x3E + 0x240), +0x3E = ground + 0x240. At the top of the arc
// (+0x14 was not negative and is not positive now) with an animation other
// than 0xFF: +0x2A = the flag's byte not 0, Sprite_EnsureAnimation(the
// animation's dword). When the frames run out: another step (+0xB) - frames
// 16 / speed again, +0xB - 1; none - MoveCmd_OpDB, phase 2. Al 1. Every
// sprite access after a call reads Sprite_Current afresh, as the original
// does.
SC6_EXPORT unsigned char __cdecl Scena06_LeapAir(unsigned char* object, unsigned, unsigned, unsigned,
                                                 unsigned gravity, unsigned animation, unsigned flag) {
    unsigned char* s = Sprite();
    Put32(s + 0x34, Get32(s + 0x34) + Get32(s + 0xC));
    Put32(s + 0x38, Get32(s + 0x38) + Get32(s + 0x10));
    W(s + 0x3E) = static_cast<std::uint16_t>(W(s + 0x3E) + (Get32(s + 0x14) >> 8));
    const std::int32_t before = Get32(s + 0x14);
    Put32(s + 0x14, before + static_cast<std::int16_t>(gravity));
    s[0xA] = static_cast<unsigned char>(s[0xA] - 1);
    const long ground = SH_CALL(AreaMap_Elevation)(Get32(s + 0x34), Get32(s + 0x38));
    s = Sprite();
    if (Get32(s + 0x14) < 0) {
        const int top = static_cast<std::int16_t>(W(s + 0x3E)) + 0x240;
        if (static_cast<std::int16_t>(ground) > top) {
            W(s + 0x3E) = static_cast<std::uint16_t>(ground + 0x240);
            s = Sprite();
        }
    }
    if (Get32(s + 0x14) <= 0 && before >= 0 && (animation & 0xFF) != 0xFF) {
        s[0x2A] = (flag & 0xFF) != 0 ? 1 : 0;
        reinterpret_cast<unsigned char (__cdecl*)(unsigned)>(reinterpret_cast<void*>(SH_CALL(Sprite_EnsureAnimation)))(animation);
        s = Sprite();
    }
    if (s[0xA] != 0) return 1;
    if (s[0xB] != 0) {
        const unsigned char speed = B(at::kMoveSpeeds + object[4]);
        if (speed == 0)
            bof3::Fatal("Scena06_LeapAir: speed 0 (Field_MoveSpeeds[%u]): the original divides by 0", (unsigned)object[4]);
        s[0xA] = static_cast<unsigned char>(16 / static_cast<int>(speed));
        Sprite()[0xB] = static_cast<unsigned char>(Sprite()[0xB] - 1);
        return 1;
    }
    SH_CALL(MoveCmd_OpDB)();
    Sprite()[4] = 2;
    return 1;
}

// original 0x54E9F0: phase 2, landing. The ground AreaMap_Elevation(x, z)
// more than 0x240 below the height +0x3E: the height (of the sprite read
// before the call) = the velocity >> 8 + the height, the velocity (of
// Sprite_Current now) += gravity, al 1. Else the ground read again, +0x3E =
// ground + 0x240, phase 0, al 0.
SC6_EXPORT unsigned char __cdecl Scena06_LeapLand(unsigned char*, unsigned, unsigned, unsigned, unsigned gravity,
                                                  unsigned, unsigned) {
    unsigned char* const s = Sprite();
    const std::uint16_t height = W(s + 0x3E);
    const long ground = SH_CALL(AreaMap_Elevation)(Get32(s + 0x34), Get32(s + 0x38));
    if (static_cast<std::int16_t>(ground) < static_cast<std::int16_t>(height) - 0x240) {
        W(s + 0x3E) = static_cast<std::uint16_t>((Get32(Sprite() + 0x14) >> 8) + height);
        unsigned char* const n = Sprite();
        Put32(n + 0x14, Get32(n + 0x14) + static_cast<std::int16_t>(gravity));
        return 1;
    }
    const long again = SH_CALL(AreaMap_Elevation)(Get32(Sprite() + 0x34), Get32(Sprite() + 0x38));
    W(Sprite() + 0x3E) = static_cast<std::uint16_t>(again + 0x240);
    Sprite()[4] = 0;
    return 0;
}

// ===========================================================================
// Slots 2 and 4

namespace {
// The hooks' starts: ScriptFlags_Set40, counter 0 `c0` and 1..3 zeroed, a
// step and a run, al 1.
unsigned char Begin(unsigned char step, unsigned char run, unsigned char c0 = 0) {
    Set40();
    SetCounter(0, c0);
    ClearCounters123();
    SetStep(step);
    SetRun(run);
    return 1;
}
bool Either(int v, int a, int b) { return v == a || v == b; }
bool Within(int v, int lo, int hi) { return v >= lo && v <= hi; }
}  // namespace

// original 0x54EA80: slot 2, the step hook (Scenario_StepHook, x and z 16.16;
// the bounds signed and inclusive, each area test reading Game_AreaNumber
// afresh). 1 when a step starts a run:
//   0x27: x 0x31.0 or 0x31.8 and z 0x27..0x29.8 - counter 0 0xA, step 5, run
//     2; else flag 7 set and 8 clear, z 0x37.0 or 0x37.8, x 0x43..0x44.8 -
//     flag 8, counters 0, step 0, run 3, and on to the next test (no answer
//     yet);
//   0x39, flag 0x10 set and 0x11 clear: z 0x52.0 or 0x52.8, x 0x27..0x32.8 -
//     run 5;
//   0x43, flag 0 clear: z 0x1F.0 or 0x1F.8, x 0x1C..0x1F.8 - run 0xC;
//   0x4D: flag 0x28 clear and 0x27 set, x 0x30.0 or 0x31.8, z 4..9.8 - run
//     0xE; else flag 0x2A clear and 0x27 set, x 0x1B.8 or 0x1C.0, z 4..9.8 -
//     counter 0 0x14, step 0x14, run 0xE;
//   0x5E: flag 0xD set and 0xE clear, x 0x43.0 or 0x43.8, z 0x32..0x34.8 -
//     step 0xA, run 4; else x 0x1B.8 or 0x1C.0 and z 0x3D..0x3F.8: flag 0x1E
//     clear - step 0x1E, run 4; set and flag 0x24 clear - counter 0 0x14,
//     step 5, run 0xA.
SC6_EXPORT unsigned char __cdecl Scena06_StepHook(int x, int z) {
    if (Area() == 0x27) {
        if (Either(x, 0x310000, 0x318000) && Within(z, 0x270000, 0x298000)) return Begin(5, 2, 0xA);
        if (Flag(7) && !Flag(8) && Either(z, 0x370000, 0x378000) && Within(x, 0x430000, 0x448000)) {
            Set(8);
            Begin(0, 3);
        }
    }
    if (Area() == 0x39 && Flag(0x10) && !Flag(0x11) && Either(z, 0x520000, 0x528000) && Within(x, 0x270000, 0x328000))
        return Begin(0, 5);
    if (Area() == 0x43 && !Flag(0) && Either(z, 0x1F0000, 0x1F8000) && Within(x, 0x1C0000, 0x1F8000)) return Begin(0, 0xC);
    if (Area() == 0x4D) {
        if (!Flag(0x28) && Flag(0x27) && Either(x, 0x300000, 0x318000) && Within(z, 0x40000, 0x98000)) return Begin(0, 0xE);
        if (!Flag(0x2A) && Flag(0x27) && Either(x, 0x1B8000, 0x1C0000) && Within(z, 0x40000, 0x98000))
            return Begin(0x14, 0xE, 0x14);
    }
    if (Area() == 0x5E) {
        if (Flag(0xD) && !Flag(0xE) && Either(x, 0x430000, 0x438000) && Within(z, 0x320000, 0x348000)) return Begin(0xA, 4);
        if (!Flag(0x1E)) {
            if (Either(x, 0x1B8000, 0x1C0000) && Within(z, 0x3D0000, 0x3F8000)) return Begin(0x1E, 4);
            return 0;
        }
        if (!Flag(0x24) && Either(x, 0x1B8000, 0x1C0000) && Within(z, 0x3D0000, 0x3F8000)) return Begin(5, 0xA, 0x14);
    }
    return 0;
}

// original 0x54EED0: slot 4, the cell hook (0x56D7A0, a cell (x, z) faced):
// 0x56D800(Scena06_Cells, 4, x, z) - the record the area and the cell match,
// or 0xFF; negative (as a signed byte) answers 0xFF; else a tail jump through
// Scena06_CellHandlers with (x, z), whose al is the answer. Entry 1 is the
// bare ret 0x437CC0: its al is what 0x56D800 left in eax, the index found.
SC6_EXPORT unsigned char __cdecl Scena06_CellHook(int x, int z) {
    const unsigned char found = SH_AT(CellFindFn, at::kCellFind)(reinterpret_cast<const void*>(at::kCells), 4, x, z);
    const int index = static_cast<signed char>(found);
    if (index < 0) return 0xFF;
    const std::uint32_t entry = Entry(at::kCellHandlers, index, at::kCellHandlerCount, "Scena06_CellHook");
    if (entry == at::kBareRet) return found;
    return reinterpret_cast<CellEntry>(static_cast<std::uintptr_t>(entry))(x, z);
}

// original 0x54EF00: Scena06_CellHandlers entry 0: run 0xB from step 0 when
// Inventory_Count(1, 0x47, 0) is 0 (its u16), else from step 5; al 1.
SC6_EXPORT unsigned char __cdecl Scena06_Cell0(void) {
    const bool none = SH_CALL(Inventory_Count)(1, 0x47, 0) == 0;
    return Begin(none ? 0 : 5, 0xB);
}

// original 0x54EF80: entry 2: flag 0x27 clear - run 0xD from step 0; al 1
// either way.
SC6_EXPORT unsigned char __cdecl Scena06_Cell2(void) {
    if (!Flag(0x27)) Begin(0, 0xD);
    return 1;
}

// original 0x54EFC0: entry 3: flag 0x27 and the story flags 5 and 6 set -
// flag 0x2C clear: run 0xF from step 5; set: step 0x1E, run 0xF (the counters
// kept). Otherwise run 0xF from step 0. Al 1.
SC6_EXPORT unsigned char __cdecl Scena06_Cell3(void) {
    if (Flag(0x27) && StoryFlag(5) && StoryFlag(6)) {
        if (!Flag(0x2C)) return Begin(5, 0xF);
        Set40();
        SetStep(0x1E);
        SetRun(0xF);
        return 1;
    }
    return Begin(0, 0xF);
}

// ===========================================================================

void ScenaSc6_Inject() {
    if (bof3::WantsShadow("scena_sc6")) scena_sc6::SelfTest();
    BOF3_INJECT(Scena06_Frame);
    BOF3_INJECT(Scena06_Start);
    BOF3_INJECT(Scena06_EnterArea);
    BOF3_INJECT(Scena06_Run);
    BOF3_INJECT(Scena06_Run01);
    BOF3_INJECT(Scena06_Run02);
    BOF3_INJECT(Scena06_Run03);
    BOF3_INJECT(Scena06_Run04);
    BOF3_INJECT(Scena06_Run05);
    BOF3_INJECT(Scena06_Run06);
    BOF3_INJECT(Scena06_Run07);
    BOF3_INJECT(Scena06_Run08);
    BOF3_INJECT(Scena06_Run09);
    BOF3_INJECT(Scena06_Run10);
    BOF3_INJECT(Scena06_Run11);
    BOF3_INJECT(Scena06_Run12);
    BOF3_INJECT(Scena06_Run13);
    BOF3_INJECT(Scena06_Run14);
    BOF3_INJECT(Scena06_MemberLines);
    BOF3_INJECT(Scena06_Run15);
    BOF3_INJECT(Scena06_Run16);
    BOF3_INJECT(Scena06_Run17);
    BOF3_INJECT(Scena06_LeaderEffect);
    BOF3_INJECT(Scena06_PartyCalls);
    BOF3_INJECT(Scena06_ObjectTrigger);
    BOF3_INJECT(Scena06_Object01);
    BOF3_INJECT(Scena06_Object02);
    BOF3_INJECT(Scena06_Object03);
    BOF3_INJECT(Scena06_Object04);
    BOF3_INJECT(Scena06_Object05);
    BOF3_INJECT(Scena06_Object06);
    BOF3_INJECT(Scena06_Object07);
    BOF3_INJECT(Scena06_Object08);
    BOF3_INJECT(Scena06_Object09);
    BOF3_INJECT(Scena06_Object10);
    BOF3_INJECT(Scena06_Object11);
    BOF3_INJECT(Scena06_Object12);
    BOF3_INJECT(Scena06_Object13);
    BOF3_INJECT(Scena06_GuestRecord);
    BOF3_INJECT(Scena06_Leap);
    BOF3_INJECT(Scena06_LeapStart);
    BOF3_INJECT(Scena06_LeapAir);
    BOF3_INJECT(Scena06_LeapLand);
    BOF3_INJECT(Scena06_StepHook);
    BOF3_INJECT(Scena06_CellHook);
    BOF3_INJECT(Scena06_Cell0);
    BOF3_INJECT(Scena06_Cell2);
    BOF3_INJECT(Scena06_Cell3);
}

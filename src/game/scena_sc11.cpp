// Scenario chapter 11 (the PSX's SCENA11.EMI), compiled into the exe at
// 0x55C040..0x55E4D2. docs/scena_sc11.md.
//
//   - Slot 0, Scena11_Frame, jumps through Scena11_States on the s8 0x8034E2:
//     0 Scena11_Start (the pass flags on, state 1), 1 Scena11_EnterArea (the
//     set-up of the area just entered, then state 2), 2 Scena11_Run, which
//     jumps through Scena11_Runs on MoveScript_Var7 to one of eight scenes.
//   - Each scene is a step machine on the u8 0x8034E5 (a switch in .text,
//     bounded): it waits on the script counter bytes 0x903848.., the message
//     request, the wait word or a stream, then calls two or three engine
//     functions and sets the next step; its last step clears the counters,
//     the step and MoveScript_Var7 (back to "no scene").
//   - Slot 1, Scena11_ObjectTrigger, calls Scena11_Triggers[object +0x86];
//     each of the fourteen handlers starts a scene at a step.
//   - Slot 3, Scena11_ArriveHook (x, z), answers in al; slot 4 answers 0xFF;
//     slot 2 is the shared Scenario_NoHook.
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. The .data
// tables (Scena11_States, Scena11_Runs, Scena11_Triggers) are read in place
// and unchecked, as Scena16_Frame / Scena16_Run read theirs: Scena11_Runs has
// ten entries and MoveScript_Var7 values of 10 and more (three triggers set
// 10, 11 and 14) run on into Scena11_Triggers (docs/scena_sc11.md section 6).
// No divergence: each function is a faithful replacement.
#include "game/scena_sc11.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/scena_sc11_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

#define SC11_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

using namespace scena_sc11;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using VoidFn = void (__cdecl*)();

constexpr std::uint32_t kState = 0x8034E2;      // s8, the chapter's state (Scena11_States)
constexpr std::uint32_t kStep = 0x8034E5;       // u8, the scene's step
constexpr std::uint32_t kCounter0 = 0x903848;   // the four script counter bytes (MoveScript_CounterOps)
constexpr std::uint32_t kCounter1 = 0x903849;
constexpr std::uint32_t kCounter2 = 0x90384A;
constexpr std::uint32_t kCounter3 = 0x90384B;
constexpr std::uint32_t kSlot = 0x903850;       // the effect slot just taken
constexpr std::uint32_t kFlagsPtr = 0x929ED0;   // the flag bits Flags_Test / Flags_Set are given
constexpr std::uint32_t kStates = 0x66166C;     // Scena11_States, 3 entries
constexpr std::uint32_t kRuns = 0x661678;       // Scena11_Runs, 10 entries (then Scena11_Triggers)
constexpr std::uint32_t kTriggers = 0x6616A0;   // Scena11_Triggers, 15 entries
constexpr std::uint32_t kRolls = 0x661648;      // Scena11_EffectRolls, 16 bytes
constexpr std::uint32_t kPartyByte = 0x90412C;  // & 0x7F: picks the call-table entries in area 0x65
constexpr std::uint32_t kLeadByte8 = 0x802D48;  // ObjTrio member 0's +8
constexpr std::uint32_t kLead89 = 0x802DC9;     // ObjTrio member 0's +0x89
constexpr std::uint32_t kLead124 = 0x802E64;    // ObjTrio member 0's +0x124, bit 0x40
constexpr std::uint32_t kMusicByte = 0x904CD0;  // a byte the area changes set (0x7C, 0x7D, 0x92, 0xFF)
constexpr std::uint32_t kByteEE0 = 0x904EE0;    // set to 0xFF with one area change
constexpr std::uint32_t kTextRecords = 0x904CE0;   // Text_Records: the item name copied in, 16 bytes

unsigned char& B(std::uint32_t address) { return At(address)[0]; }

// A table of code pointers in the exe's data, read as the original reads it:
// afresh, and indexed without a bound.
VoidFn Entry(std::uint32_t table, int index) {
    return reinterpret_cast<VoidFn>(static_cast<std::uintptr_t>(
        static_cast<std::uint32_t>(Long(At(table + 4u * static_cast<std::uint32_t>(index))))));
}
unsigned char* Flags() { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(At(kFlagsPtr))))); }
unsigned char* EffectRecord(unsigned slot) { return Effect_Objects + ((slot & 0xFFu) << 7); }

void CallA(unsigned n) { SH_CALL(Scenario_CallA)(n); }
void CallB(unsigned n) { SH_CALL(Scenario_CallB)(n); }
unsigned char FindFree() { return SH_CALL(Effect_FindFree)(); }
void ChangeArea(unsigned area, int x, int z, unsigned flags) { SH_CALL(Field_ChangeArea)(area, x, z, flags); }

// The scenes' common end: counters 1..3, the step and MoveScript_Var7 cleared
// (and counter 0 by the callers that clear it first).
void EndScene() {
    B(kCounter1) = 0;
    B(kCounter2) = 0;
    B(kCounter3) = 0;
    B(kStep) = 0;
    MoveScript_Var7 = 0;
}
void EndSceneAll() {
    B(kCounter0) = 0;
    EndScene();
}

// An item's 16-byte name record copied into Text_Records, dword by dword.
void CopyName(const unsigned char* name) {
    for (unsigned i = 0; i < 16; i += 4) SetLong(At(kTextRecords + i), Long(name + i));
}

// The slot just taken (kSlot, read back): live, `kind`, at (x, Camera_Angles[1], z),
// +9 = `nine`.
unsigned char* Place(unsigned char kind, std::int32_t x, std::int32_t z, unsigned char nine) {
    unsigned char* const e = EffectRecord(B(kSlot));
    e[0] = 1;
    e[5] = kind;
    SetLong(e + 0x64, x);
    SetLong(e + 0x68, Camera_Angles[1]);
    SetLong(e + 0x6C, z);
    e[9] = nine;
    return e;
}

// Effect_FindFree into kSlot; false when none is free (0xFF).
bool TakeSlot() {
    const unsigned char s = FindFree();
    B(kSlot) = s;
    return s != 0xFF;
}

bool EffectLive(unsigned slot) { return EffectRecord(slot)[0] != 0; }

}  // namespace

// ===========================================================================
// Slot 0 and its states

// original 0x55C040: chapter 11's vtable slot 0 (PSX 0x801F7288): a tail jump
// through Scena11_States on the s8 0x8034E2 - 0 Scena11_Start, 1
// Scena11_EnterArea, 2 Scena11_Run. Unchecked.
SC11_EXPORT void __cdecl Scena11_Frame(void) { Entry(kStates, static_cast<signed char>(B(kState)))(); }

// original 0x55C050: state 0: the pass flags 0x1F, state 1.
SC11_EXPORT void __cdecl Scena11_Start(void) {
    Draw_PassFlags = 0x1F;
    B(kState) = 1;
}

namespace {

// Area 0x65, counter 2 at 0, flag 0xC set and 0xE not: the script flags' bit
// 0x40, the counters, pass flags, step 0 and MoveScript_Var7 8 (scene 8), then
// the call-table entries (B, then A) by the party byte & 0x7F: 7..18.
void Area65Start() {
    SH_CALL(ScriptFlags_Set40)();
    const unsigned party = B(kPartyByte) & 0x7Fu;
    B(kCounter0) = 0;
    B(kCounter1) = 0;
    B(kCounter2) = 0;
    B(kCounter3) = 0;
    Draw_PassFlags = 0;
    B(kStep) = 0;
    MoveScript_Var7 = 8;
    switch (party) {
    case 7: CallB(1); CallB(0); CallA(0); return;
    case 8: CallB(1); CallA(1); return;
    case 9: CallB(1); CallB(2); CallA(0); return;
    case 10: CallB(1); CallA(2); return;
    case 13: CallB(0); CallA(1); return;
    case 14: CallB(0); CallB(2); CallA(0); return;
    case 15: CallB(0); CallA(2); return;
    case 16: CallB(2); CallA(1); return;
    case 18: CallB(2); CallA(2); return;
    default: return;   // 11, 12, 17 and anything outside 7..18
    }
}

}  // namespace

// original 0x55C060: state 1 (PSX SCENA11's): the set-up of the area just
// entered, by Game_AreaNumber and counter 2, then state 2. Areas 0x65, 0x79,
// 0x82, 0x83, 0x84 and 0x88 each have one; 0x2D, 0x41, 0x57, 0x10 and 0x73
// clear the counters, the step and MoveScript_Var7.
// As the original has it: the area is read afresh after the calls; area
// 0x83's counter 1 raises the elevation and goes on to the 0x84 test; area
// 0x88's cases go on to the clearing test, its other counters do not; the
// effect of area 0x88's counter 10 is indexed by the slot as answered (not
// read back).
SC11_EXPORT void __cdecl Scena11_EnterArea(void) {
    if (Game_AreaNumber == 0x65) {
        switch (B(kCounter2)) {
        case 0:
            if (SH_CALL(Flags_Test)(Flags(), 0xC) != 0 && SH_CALL(Flags_Test)(Flags(), 0xE) == 0) Area65Start();
            break;
        case 1:
            Draw_PassFlags = 0x1F;
            B(kState) = 2;
            return;
        case 2:
            Draw_PassFlags = 0;
            B(kState) = 2;
            return;
        default:
            B(kState) = 2;
            return;
        }
    }
    const unsigned area = Game_AreaNumber;
    if (area == 0x79) {
        if (B(kCounter2) == 1) Draw_PassFlags = 0;
        B(kState) = 2;
        return;
    }
    if (area == 0x82) {
        if (B(kCounter2) == 1) Draw_PassFlags = 0x1F;
        B(kState) = 2;
        return;
    }
    if (area == 0x83) {
        switch (B(kCounter2)) {
        case 1:
            SH_CALL(MapView_SetElevation)(0x498);
            MapView_Redraw = 2;
            break;
        case 2:
            Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags | 0x16);
            Draw_PassFlags = 0x1F;
            B(kState) = 2;
            return;
        case 3:
            Draw_PassFlags = 0x1F;
            B(kState) = 2;
            return;
        default:
            B(kState) = 2;
            return;
        }
    }
    const unsigned again = Game_AreaNumber;
    if (again == 0x84) {
        switch (B(kCounter2)) {
        case 1:
            Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags | 0x16);
            Draw_PassFlags = 0;
            break;
        case 2:
            Draw_PassFlags = 0x1F;
            break;
        default:
            break;
        }
        B(kState) = 2;
        return;
    }
    if (again == 0x88) {
        switch (B(kCounter2)) {
        case 1:
            CallB(3);
            break;
        case 2:
            SH_CALL(MapView_SetElevation)(0x200);
            MapView_Redraw = 2;
            break;
        case 10: {
            const unsigned char slot = FindFree();
            if (slot != 0xFF) {
                unsigned char* const e = EffectRecord(slot);
                e[0] = 1;
                e[5] = 0x77;
                e[1] = 1;
            }
            break;
        }
        default:
            B(kState) = 2;
            return;
        }
    }
    const unsigned last = Game_AreaNumber;
    if (last == 0x2D || last == 0x41 || last == 0x57 || last == 0x10 || last == 0x73) {
        B(kCounter0) = 0;
        B(kCounter1) = 0;
        B(kCounter2) = 0;
        B(kCounter3) = 0;
        B(kStep) = 0;
        MoveScript_Var7 = 0;
    }
    B(kState) = 2;
}

// original 0x55C360: state 2: a tail jump through Scena11_Runs on
// MoveScript_Var7 (s8, unchecked) - 0 and 7 a bare ret, 1..6, 8, 9 the
// scenes. Ten entries; 10.. read Scena11_Triggers' words.
SC11_EXPORT void __cdecl Scena11_Run(void) { Entry(kRuns, MoveScript_Var7)(); }

// ===========================================================================
// The scenes (each a switch on the step 0x8034E5)

// original 0x55C370: scene 1, steps 0..15, 0x32, 0x33, 0x3C (above 0x3C, and
// the steps between, nothing). Step 0 drops the party in by member 0's +8;
// the area changes to 0x83 and 0x9C and back, each waited on through counter
// 0; flags 1, 2, 0 and 4 set; 0x32 and 0x3C open script messages 6 and 7,
// 0x33 waits for the message to close.
// As the original has it: step 11 with Field_Kind2Hold set runs step 12's
// test; each area change's stores precede the call and its trailing byte
// follows it.
SC11_EXPORT void __cdecl Scena11_Scene1(void) {
    switch (B(kStep)) {
    case 0: {
        const unsigned lead = B(kLeadByte8);
        Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags | 8);
        B(kStep) = 1;
        if (lead == 0) SH_CALL(Party_DropIn)(4);
        else if (lead == 6) SH_CALL(Party_DropIn)(3);
        else if (lead == 7) SH_CALL(Party_DropIn)(1);
        return;
    }
    case 1:
        if (B(kCounter0) != 1) return;
        SH_CALL(Transition_Start)(0xD);
        B(kStep) = 2;
        return;
    case 2:
        if (MoveScript_WaitWordDA != 0) return;
        Draw_PassFlags = 0;
        B(kStep) = 3;
        return;
    case 3:
        if (B(kCounter0) != 2) return;
        B(kCounter0) = 0;
        B(kCounter2) = 2;
        B(kStep) = 4;
        ChangeArea(0x83, 0x50000, 0x5D0000, 0x83);
        B(kByteEE0) = 0xFF;
        return;
    case 4:
        if (B(kCounter0) != 1) return;
        B(kCounter0) = 0;
        B(kCounter2) = 0;
        Camera_ShiftY = 0;
        MapView_Redraw = 2;
        B(kStep) = 5;
        ChangeArea(0x9C, 0x190000, 0x190000, 0x80);
        B(kMusicByte) = 0x92;
        return;
    case 5: B(kStep) = 6; return;
    case 6: B(kStep) = 7; return;
    case 7: B(kStep) = 8; return;
    case 8: {
        if (B(kCounter0) != 1) return;
        unsigned char* const f = Flags();
        B(kCounter0) = 0;
        B(kStep) = 9;
        SH_CALL(Flags_Set)(f, 1);
        ChangeArea(0x83, 0x50000, 0x5D0000, 0x84);
        return;
    }
    case 9:
        if (B(kCounter0) != 2) return;
        B(kCounter0) = 0;
        B(kCounter2) = 1;
        B(kStep) = 0xA;
        ChangeArea(0x83, 0x200000, 0x2F0000, 0x85);
        return;
    case 10:
        SH_CALL(Kind2_Place)(0);
        SH_CALL(MapView_SetElevation)(0x498);
        MapView_Redraw = 2;
        B(kStep) = 0xB;
        return;
    case 11:
        if (Field_Kind2Hold == 0) {
            B(kCounter0) = 1;
            B(kStep) = 0xC;
            return;
        }
        [[fallthrough]];
    case 12:
        if (B(kCounter0) != 2) return;
        SH_CALL(Transition_Start)(0xD);
        B(kStep) = 0xD;
        return;
    case 13:
        if (MoveScript_WaitWordDA != 0) return;
        Draw_PassFlags = 0;
        B(kStep) = 0xE;
        return;
    case 14: {
        if (B(kCounter0) != 3) return;
        B(kCounter2) = 3;
        unsigned char* const f = Flags();
        B(kCounter0) = 0;
        B(kStep) = 0xF;
        SH_CALL(Flags_Set)(f, 2);
        ChangeArea(0x83, 0x70000, 0x5C0000, 0x86);
        return;
    }
    case 15:
        if (B(kCounter0) != 0x14) return;
        SH_CALL(Flags_Set)(Flags(), 0);
        SH_CALL(Flags_Set)(Flags(), 4);
        SH_CALL(ScriptFlags_Clear40)();
        Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags ^ 8);
        EndSceneAll();
        return;
    case 0x32:
        SH_CALL(Msg_OpenScript)(6);
        Field_Request = 2;
        B(kStep) = 0x33;
        return;
    case 0x33:
        if (Field_Request == 2) return;
        SH_CALL(ScriptFlags_Clear40)();
        B(kStep) = 0;
        MoveScript_Var7 = 0;
        return;
    case 0x3C:
        SH_CALL(Msg_OpenScript)(7);
        Field_Request = 2;
        B(kStep) = 0x33;
        return;
    default:
        return;
    }
}

// original 0x55C750: scene 2, steps 0 and 1: once the message is closed,
// counter 0 at 1 and the party dropped in (member 0); then, at counter 0 3,
// flag 5 set and the scene ended (counter 0 kept).
SC11_EXPORT void __cdecl Scena11_Scene2(void) {
    switch (B(kStep)) {
    case 0:
        if (Field_Request == 2) return;
        B(kCounter0) = 1;
        SH_CALL(Party_DropIn)(0);
        B(kStep) = 1;
        return;
    case 1:
        if (B(kCounter0) != 3) return;
        SH_CALL(Flags_Set)(Flags(), 5);
        SH_CALL(ScriptFlags_Clear40)();
        EndScene();
        return;
    default:
        return;
    }
}

// original 0x55C7D0: scene 3, steps 0, 1, 10, 11, 12, 15, 16. Step 0 drops
// the party in (4), step 1 waits for counter 0 6 and sets flag 6. Step 10
// asks for items 0x23, 0x24, 0x56 and 0x4D (category 0, not equipped): all
// held, step 15; else step 11, script message 0x35, step 12 waits and ends.
// Step 15 drops the party in (5), step 16 waits for counter 0 5 and sets
// flag 9.
SC11_EXPORT void __cdecl Scena11_Scene3(void) {
    switch (B(kStep)) {
    case 0:
        if (Field_Request == 2) return;
        B(kCounter0) = 1;
        SH_CALL(Party_DropIn)(4);
        B(kStep) = 1;
        return;
    case 1:
        if (B(kCounter0) != 6) return;
        SH_CALL(Flags_Set)(Flags(), 6);
        SH_CALL(ScriptFlags_Clear40)();
        EndScene();
        return;
    case 10:
        if (Field_Request == 2) return;
        if (SH_CALL(Inventory_Count)(0, 0x23, 0) != 0 && SH_CALL(Inventory_Count)(0, 0x24, 0) != 0 &&
            SH_CALL(Inventory_Count)(0, 0x56, 0) != 0 && SH_CALL(Inventory_Count)(0, 0x4D, 0) != 0) {
            B(kStep) = 0xF;
            return;
        }
        B(kStep) = 0xB;
        return;
    case 11:
        SH_CALL(Msg_OpenScript)(0x35);
        Field_Request = 2;
        B(kStep) = 0xC;
        return;
    case 12:
        if (Field_Request == 2) return;
        if (B(kCounter0) != 6) return;
        SH_CALL(ScriptFlags_Clear40)();
        EndSceneAll();
        return;
    case 15:
        B(kCounter0) = 1;
        SH_CALL(Party_DropIn)(5);
        B(kStep) = 0x10;
        return;
    case 16:
        if (Field_Request == 2) return;
        if (B(kCounter0) != 5) return;
        SH_CALL(ScriptFlags_Clear40)();
        SH_CALL(Flags_Set)(Flags(), 9);
        EndScene();
        return;
    default:
        return;
    }
}

// original 0x55CC50: effect slot `slot` made the current sprite, animation
// bank 0x18, its fields +0x24, +0x2A, +0x48, +0x5D..+0x5F cleared, +0x2B up
// one, +0x14 0, +0x20 -8, animation 0xE, +1 3 and +2 1.
// As the original has it: Sprite_Current is read afresh for every field.
SC11_EXPORT void __cdecl Scena11_EffectAnimate(unsigned slot) {
    Sprite_Current = EffectRecord(slot);
    SH_CALL(Sprite_SetAnimationBank)(0x18);
    Sprite_Current[0x24] = 0;
    Sprite_Current[0x2A] = 0;
    Sprite_Current[0x2B] = static_cast<unsigned char>(Sprite_Current[0x2B] + 1);
    Sprite_Current[0x48] = 0;
    SetLong(Sprite_Current + 0x14, 0);
    SetLong(Sprite_Current + 0x20, -8);
    Sprite_Current[0x5D] = 0;
    Sprite_Current[0x5E] = 0;
    Sprite_Current[0x5F] = 0;
    SH_CALL(Sprite_SetAnimation)(0xE);
    Sprite_Current[1] = 3;
    Sprite_Current[2] = 1;
}

// original 0x55C980: scene 4, steps 0, 1, 2, 5, 6, 0x14, 0x15, 0x32. Step 0,
// with member 0's +0x89 at 6: an effect (kind 0x34 at 0x250000, 0xB0000,
// 0x5000000) whose +1 is Scena11_EffectRolls[Rand & 15]; by that roll 0 / 4
// step 0x32, 2 step 1, 5 Scena11_EffectAnimate and step 5, 1 / 3 nothing.
// Step 1 opens system message 0xD9 with member 0 current; step 5 adds nine of
// item 0x23 (full: its name and system message 3; else script message 0x42);
// step 0x14, with +0x89 at 7, sound 0x106 and script message 0; step 0x15
// adds one of item 0x56; step 0x32 ends the scene.
// As the original has it: the slot is read back from kSlot after Rand; a roll
// above 5 does nothing; a full pool at step 0 leaves the step.
SC11_EXPORT void __cdecl Scena11_Scene4(void) {
    switch (B(kStep)) {
    case 0: {
        if (B(kLead89) != 6) {
            B(kStep) = 0x32;
            return;
        }
        if (!TakeSlot()) return;
        unsigned char* const e = EffectRecord(B(kSlot));
        e[0] = 1;
        e[5] = 0x34;
        SetLong(e + 0x34, 0x250000);
        SetLong(e + 0x38, 0xB0000);
        SetLong(e + 0x3C, 0x5000000);
        const unsigned roll = static_cast<unsigned>(SH_CALL(Rand)()) & 0xFu;
        const unsigned char kind = At(kRolls)[roll];
        EffectRecord(B(kSlot))[1] = kind;
        switch (kind) {
        case 0:
        case 4:
            B(kStep) = 0x32;
            return;
        case 2:
            B(kStep) = 1;
            return;
        case 5:
            SH_CALL(Scena11_EffectAnimate)(B(kSlot));
            B(kStep) = 5;
            return;
        default:
            return;
        }
    }
    case 1:
        Sprite_Current = ObjTrio;
        Field_State = ObjTrio;
        SH_AT(void (__cdecl*)(unsigned), kSpriteSetUp)(0);
        SH_AT(void (__cdecl*)(unsigned, unsigned), kCountDown)(1, 6);
        SH_CALL(Msg_OpenSystem)(0xD9);
        Field_Request = 2;
        B(kStep) = 2;
        return;
    case 2:
        if (Field_Request == 2) return;
        B(kStep) = 0x32;
        return;
    case 5:
        if (SH_CALL(Inventory_Add)(0, 0x23, 9) == 0) {
            CopyName(SH_CALL(Item_NamePtr)(0, 0x23));
            SH_CALL(Msg_OpenSystem)(3);
            Field_Request = 2;
            B(kStep) = 2;
            return;
        }
        SH_CALL(Msg_OpenScript)(0x42);
        Field_Request = 2;
        B(kStep) = 6;
        return;
    case 6:
        if (Field_Request == 2) return;
        SH_CALL(Sound_PlayEffect)(0x106);
        B(kStep) = 0x32;
        return;
    case 0x14:
        if (B(kLead89) != 7) {
            B(kStep) = 0x32;
            return;
        }
        SH_CALL(Sound_PlayEffect)(0x106);
        SH_CALL(Msg_OpenScript)(0);
        Field_Request = 2;
        B(kStep) = 0x15;
        return;
    case 0x15:
        if (Field_Request == 2) return;
        SH_CALL(Inventory_Add)(0, 0x56, 1);
        B(kStep) = 0x32;
        return;
    case 0x32:
        SH_CALL(ScriptFlags_Clear40)();
        EndSceneAll();
        return;
    default:
        return;
    }
}

// original 0x55CCF0: scene 5, steps 0..6, 8..13, 15..20, 22, 0x28..0x2A (the
// rest nothing). Party drop-ins (1, 3, 5, 9) waited on through counter 0; the
// music faded; flags 7 and 8; area 0x44 entered four ways; at step 6 two
// effects (kinds 0x8C, 0x8B) and member 0's +0x124 bit 0x40, cleared again at
// steps 8 and 15; at step 9 an effect (kind 6) on member 0's position; the
// streams 6 and 5 played and waited on; item 0x24 (four) given, its name
// shown if the bag is full; 0x28 ends at once when counter 0 is 0xFF.
// As the original has it: step 4 and step 0x2A share one test (counter 0 3:
// drop-in 3 and step 2; 0x14: step 5); step 13 and step 22 share the end
// (counter 0 kept); step 9 reads Sprite_Current again after the call and
// the slot byte afresh for every field; a full pool at step 9 leaves the
// step.
SC11_EXPORT void __cdecl Scena11_Scene5(void) {
    switch (B(kStep)) {
    case 0:
        if (Field_Request == 2) return;
        B(kCounter0) = 1;
        SH_CALL(Party_DropIn)(1);
        B(kStep) = 1;
        return;
    case 1:
        if (B(kCounter0) != 3) return;
        B(kCounter0) = 0;
        SH_CALL(Music_FadeOutStop)(0x1E);
        SH_CALL(Flags_Set)(Flags(), 7);
        B(kStep) = 2;
        ChangeArea(0x44, 0x300000, 0x198000, 0x82);
        B(kMusicByte) = 0x7D;
        return;
    case 2:
        if (B(kCounter0) != 5) return;
        B(kStep) = 3;
        return;
    case 3:
        if (B(kCounter0) != 9) return;
        B(kStep) = 4;
        return;
    case 4:
    case 0x2A: {
        const unsigned char c = B(kCounter0);
        if (c == 3) {
            SH_CALL(Party_DropIn)(3);
            B(kStep) = 2;
        } else if (c == 0x14) {
            B(kStep) = 5;
        }
        return;
    }
    case 5:
        if (Field_Request == 2) return;
        B(kStep) = 6;
        return;
    case 6:
        if (TakeSlot()) {
            unsigned char* const e = EffectRecord(B(kSlot));
            e[0] = 1;
            e[5] = 0x8C;
        }
        if (TakeSlot()) {
            unsigned char* const e = EffectRecord(B(kSlot));
            e[0] = 1;
            e[5] = 0x8B;
        }
        B(kStep) = 7;
        Field_State = ObjTrio;
        Sprite_Current = ObjTrio;
        B(kLead124) = static_cast<unsigned char>(B(kLead124) | 0x40);
        return;
    case 8:
        Field_State = ObjTrio;
        Sprite_Current = ObjTrio;
        B(kLead124) = static_cast<unsigned char>(B(kLead124) & 0xBF);
        SH_CALL(Sound_PlayEffect)(0x201);
        SH_CALL(Music_FadeOutStop)(0x1E);
        B(kStep) = 9;
        return;
    case 9: {
        Sprite_Current = ObjTrio;
        const unsigned char s = FindFree();
        Sprite_Current[0xB] = s;
        unsigned char* const p = Sprite_Current;
        if (p[0xB] == 0xFF) return;
        EffectRecord(p[0xB])[0] = 1;
        EffectRecord(p[0xB])[5] = 6;
        EffectRecord(p[0xB])[6] = 3;
        SetLong(EffectRecord(p[0xB]) + 0xC, 0);
        SetLong(EffectRecord(p[0xB]) + 0x10, 0x3A);
        {
            unsigned char* const e = EffectRecord(p[0xB]);
            SetWord(e + 0x2E, Word(p + 0x2E));
        }
        unsigned char* const e = EffectRecord(p[0xB]);
        const unsigned w = Word(p + 0x30);
        B(kStep) = 0xA;
        SetWord(e + 0x30, w);
        return;
    }
    case 10:
        SH_CALL(Sound_LoadStream)(6);
        B(kStep) = 0xB;
        return;
    case 11:
        if (SH_CALL(Sound_StreamDone)() == 0) return;
        SH_CALL(Msg_OpenScript)(0x28);
        Field_Request = 2;
        B(kStep) = 0xC;
        return;
    case 12:
        if (Field_Request == 2) return;
        B(kStep) = 0xD;
        ChangeArea(0x44, 0x2E8000, 0x178000, 0x84);
        B(kMusicByte) = 0x7C;
        return;
    case 13:
        if (B(kCounter0) != 1) return;
        SH_CALL(ScriptFlags_Clear40)();
        B(kStep) = 0;
        MoveScript_Var7 = 0;
        return;
    case 15:
        Field_State = ObjTrio;
        Sprite_Current = ObjTrio;
        B(kLead124) = static_cast<unsigned char>(B(kLead124) & 0xBF);
        SH_CALL(Music_FadeOutStop)(0x1E);
        SH_CALL(Party_DropIn)(5);
        B(kStep) = 0x10;
        return;
    case 16:
        SH_CALL(Sound_LoadStream)(5);
        B(kStep) = 0x11;
        return;
    case 17:
        if (SH_CALL(Sound_StreamDone)() == 0) return;
        B(kCounter0) = 2;
        B(kStep) = 0x12;
        return;
    case 18: {
        if (B(kCounter0) != 5) return;
        unsigned char* const f = Flags();
        B(kCounter0) = 0;
        B(kStep) = 0x13;
        SH_CALL(Flags_Set)(f, 8);
        ChangeArea(0x44, 0x2E8000, 0x178000, 0x86);
        B(kMusicByte) = 0x7C;
        return;
    }
    case 19:
        if (B(kCounter0) != 1) return;
        if (SH_CALL(Inventory_Add)(0, 0x24, 4) == 0) {
            CopyName(SH_CALL(Item_NamePtr)(0, 0x24));
            SH_CALL(Msg_OpenScript)(0x33);
            Field_Request = 2;
            B(kStep) = 0x14;
            return;
        }
        SH_CALL(Msg_OpenScript)(0x32);
        Field_Request = 2;
        SH_CALL(Sound_PlayEffect)(0x106);
        B(kStep) = 0x14;
        return;
    case 20:
        if (Field_Request == 2) return;
        B(kCounter0) = 2;
        B(kStep) = 0x16;
        return;
    case 22:
        if (B(kCounter0) != 3) return;
        SH_CALL(ScriptFlags_Clear40)();
        B(kStep) = 0;
        MoveScript_Var7 = 0;
        return;
    case 0x28:
        if (Field_Request == 2) return;
        if (B(kCounter0) == 0xFF) {
            SH_CALL(ScriptFlags_Clear40)();
            B(kCounter0) = 0;
            B(kStep) = 0;
            MoveScript_Var7 = 0;
            return;
        }
        B(kCounter0) = 0;
        SH_CALL(Party_DropIn)(9);
        B(kStep) = 0x29;
        return;
    case 0x29:
        if (B(kCounter0) != 1) return;
        B(kCounter0) = 0;
        B(kStep) = 0x2A;
        SH_CALL(Music_FadeOutStop)(0x1E);
        ChangeArea(0x44, 0x300000, 0x198000, 0x88);
        B(kMusicByte) = 0x7D;
        return;
    default:
        return;
    }
}

// original 0x55D280: scene 6, steps 0, 1, 5..7, 10..14, 20, 30..35, 40..48.
// Area 0x82 entered by several ways (flag 0xA, then by counter 3 at 1 or
// not); requests 8 with the pass flags off; streams 7, 6 and 2 played and
// waited on; flags 0xB and 0xC; the music track 0x83. Steps 7 and 14 share
// one end (counter 0 at 3), step 35 ends through 0x537480(0xFFFF, 7), step 48
// through flag 0xC.
// As the original has it: step 10 sets the script flags' bit 0x40 before the
// request test; steps 1 and 11 set the request and leave the step.
SC11_EXPORT void __cdecl Scena11_Scene6(void) {
    switch (B(kStep)) {
    case 0:
        if (Field_Request == 2) return;
        B(kCounter0) = 1;
        SH_CALL(Party_DropIn)(1);
        B(kStep) = 1;
        return;
    case 1:
        if (B(kCounter0) != 7) return;
        if (Field_Request == 2) return;
        Field_Request = 8;
        Draw_PassFlags = 0;
        return;
    case 5: {
        if (Field_Request != 0) return;
        unsigned char* const f = Flags();
        B(kStep) = 6;
        B(kCounter0) = 0;
        SH_CALL(Flags_Set)(f, 0xA);
        ChangeArea(0x82, 0x290000, 0x540000, 0x82);
        return;
    }
    case 6:
        Draw_PassFlags = 0x1F;
        B(kStep) = 7;
        return;
    case 7:
    case 14:
        if (B(kCounter0) != 3) return;
        SH_CALL(ScriptFlags_Clear40)();
        EndScene();
        return;
    case 10:
        SH_CALL(ScriptFlags_Set40)();
        if (Field_Request == 2) return;
        SH_CALL(Transition_Start)(0xD);
        SH_CALL(Music_FadeOutStop)(0x1E);
        B(kStep) = 0xB;
        return;
    case 11:
        if (MoveScript_WaitWordDA != 0) return;
        Field_Request = 8;
        Draw_PassFlags = 0;
        SH_CALL(Port_DroppedCall)(1);
        return;
    case 12:
        if (Field_Request != 0) return;
        B(kStep) = 0xD;
        B(kCounter0) = 0;
        ChangeArea(0x82, 0x290000, 0x540000, 0x82);
        return;
    case 13:
        Draw_PassFlags = 0x1F;
        B(kStep) = 0xE;
        return;
    case 20:
        if (Field_Request != 0) return;
        B(kCounter0) = 0;
        SH_CALL(Flags_Set)(Flags(), 0xA);
        if (B(kCounter3) == 1) {
            B(kStep) = 0x1E;
            ChangeArea(0x82, 0x290000, 0x540000, 0x83);
            B(kMusicByte) = 0xFF;
            return;
        }
        B(kStep) = 0x28;
        ChangeArea(0x82, 0x290000, 0x540000, 0x84);
        return;
    case 30:
        Draw_PassFlags = 0x1F;
        B(kStep) = 0x1F;
        return;
    case 31:
        if (B(kCounter0) != 1) return;
        SH_CALL(Sound_LoadStream)(7);
        B(kStep) = 0x20;
        return;
    case 32:
        if (SH_CALL(Sound_StreamDone)() == 0) return;
        B(kStep) = 0x21;
        return;
    case 33:
        if (B(kCounter0) != 4) return;
        SH_CALL(Sound_LoadStream)(6);
        B(kStep) = 0x22;
        return;
    case 34:
        if (SH_CALL(Sound_StreamDone)() == 0) return;
        B(kCounter0) = 5;
        B(kStep) = 0x23;
        return;
    case 35:
        if (B(kCounter0) != 6) return;
        SH_AT(void (__cdecl*)(unsigned, unsigned), kCountDown)(0xFFFF, 7);
        SH_CALL(ScriptFlags_Clear40)();
        EndScene();
        return;
    case 40:
        Draw_PassFlags = 0x1F;
        B(kStep) = 0x29;
        return;
    case 41:
        if (B(kCounter0) != 5) return;
        SH_CALL(Port_DroppedCall)(1);
        B(kStep) = 0x2A;
        return;
    case 42:
        if (B(kCounter0) != 0xA) return;
        SH_CALL(Music_FadeOutStop)(0x1E);
        B(kCounter0) = 0;
        B(kStep) = 0x2B;
        SH_CALL(Transition_Start)(0xD);
        return;
    case 43:
        if (MoveScript_WaitWordDA != 0) return;
        Draw_PassFlags = 0;
        B(kStep) = 0x2C;
        return;
    case 44:
        SH_CALL(Msg_OpenScript)(0x40);
        Field_Request = 2;
        B(kStep) = 0x2D;
        return;
    case 45: {
        if (Field_Request == 2) return;
        unsigned char* const f = Flags();
        B(kStep) = 0x2E;
        B(kCounter2) = 1;
        SH_CALL(Flags_Set)(f, 0xB);
        ChangeArea(0x82, 0x290000, 0x530000, 0x85);
        SH_CALL(Music_Play)(0x83, 8);
        return;
    }
    case 46:
        if (B(kCounter0) != 4) return;
        SH_AT(void (__cdecl*)(), kSoundJmp)();
        SH_CALL(Sound_LoadStream)(2);
        B(kStep) = 0x2F;
        return;
    case 47:
        if (SH_CALL(Sound_StreamDone)() == 0) return;
        SH_CALL(Sound_ResumeAll)();
        B(kStep) = 0x30;
        return;
    case 48:
        if (B(kCounter0) != 0xA) return;
        SH_CALL(Flags_Set)(Flags(), 0xC);
        SH_CALL(ScriptFlags_Clear40)();
        EndScene();
        return;
    default:
        return;
    }
}

// original 0x55D700: scene 8 (MoveScript_Var7 8, which Scena11_EnterArea sets
// in area 0x65), steps 0..39 (5, 23..26, 28, 30, 32 nothing). Areas 0x65,
// 0x84 and 0x83 entered in turn; Kind2_Place 0, 1, 2; the camera pulled in
// (Camera_Distance - 0x500, then - 0x780); effects of kind 0x13 and 0x31
// placed at (x, Camera_Angles[1], z), each later waited on until its record
// is free (counter 3 holds the slot); flags 0xE..0x12; the scene ends at
// counter 0 0x15.
// As the original has it: step 7 with the effect still live runs step 8's
// body; step 14 goes on after a full pool (its second effect and the
// MoveCmd_TestFB call regardless); a full pool elsewhere leaves the step.
SC11_EXPORT void __cdecl Scena11_Scene8(void) {
    switch (B(kStep)) {
    case 0: {
        unsigned char* const f = Flags();
        B(kStep) = 1;
        SH_CALL(Flags_Set)(f, 0xE);
        ChangeArea(0x65, 0x240000, 0x250000, 0x80);
        return;
    }
    case 1:
        SH_CALL(Kind2_Place)(0);
        Camera_Distance = static_cast<short>(Camera_Distance + 0xFB00);
        MapView_Redraw = 2;
        if (!TakeSlot()) return;
        B(kStep) = 2;
        Place(0x13, -0x1A2, 0x200, 4);
        return;
    case 2:
        Draw_PassFlags = 0x1F;
        B(kStep) = 3;
        return;
    case 3: {
        Field_Kind2X = 0x220000;
        Field_Kind2Z = 0x230000;
        MoveScript_F3Divisor = 4;
        if (!TakeSlot()) return;
        B(kStep) = 4;
        unsigned char* const e = Place(0x31, -0x2AA, 0x200, 0xFF);
        SetLong(e + 0xC, 0);
        return;
    }
    case 4:
        if (Field_Kind2Hold != 0) return;
        B(kCounter0) = 1;
        B(kStep) = 6;
        return;
    case 6:
        if (B(kCounter0) != 4) return;
        if (!TakeSlot()) return;
        B(kCounter3) = B(kSlot);
        B(kStep) = 7;
        Place(0x13, -0x2EC, 0x200, 0xA);
        return;
    case 7:
        if (!EffectLive(B(kCounter3))) {
            B(kCounter3) = 0;
            B(kCounter0) = 5;
            B(kStep) = 8;
            return;
        }
        [[fallthrough]];
    case 8:
        if (B(kCounter0) != 0xA) return;
        if (!TakeSlot()) return;
        B(kStep) = 9;
        Place(0x13, -0x2AA, 0x200, 0xA);
        return;
    case 9:
        if (B(kCounter0) != 0x14) return;
        SH_CALL(Transition_Start)(0xD);
        B(kStep) = 0xA;
        return;
    case 10:
        if (MoveScript_WaitWordDA != 0) return;
        Draw_PassFlags = 0;
        B(kStep) = 0xB;
        return;
    case 11:
        SH_CALL(Msg_OpenScript)(4);
        Field_Request = 2;
        B(kStep) = 0xC;
        return;
    case 12:
        if (Field_Request == 2) return;
        B(kStep) = 0xD;
        B(kCounter0) = 0;
        B(kCounter2) = 1;
        ChangeArea(0x65, 0x490000, 0x90000, 0x81);
        return;
    case 13:
        if (B(kCounter0) != 0x14) return;
        B(kCounter0) = 0;
        B(kCounter2) = 1;
        B(kStep) = 0xE;
        ChangeArea(0x84, 0x530000, 0x1B0000, 0x83);
        return;
    case 14:
        SH_CALL(Sound_PlayEffect)(0x200);
        SH_CALL(Kind2_Place)(1);
        Camera_Distance = static_cast<short>(Camera_Distance + 0xF880);
        MapView_Redraw = 2;
        if (TakeSlot()) {
            B(kCounter3) = B(kSlot);
            B(kStep) = 0xF;
            Place(0x13, -0x3C8, 0x258, 4);
        }
        SH_CALL(MoveCmd_TestFB)(0x53, 0x1B);
        if (TakeSlot()) {
            unsigned char* const e = EffectRecord(B(kSlot));
            e[0] = 1;
            e[5] = 0x18;
            e[1] = 0x5F;
        }
        Cond_ByteFE = 0x23;
        return;
    case 15:
        if (EffectLive(B(kCounter3))) return;
        B(kCounter3) = 0;
        SH_CALL(Msg_OpenScript)(0x1A);
        Field_Request = 2;
        B(kStep) = 0x10;
        return;
    case 16:
        if (Field_Request == 2) return;
        SH_CALL(Transition_Start)(0xD);
        B(kStep) = 0x11;
        return;
    case 17:
        if (MoveScript_WaitWordDA != 0) return;
        B(kCounter0) = 1;
        Draw_PassFlags = 0x1F;
        B(kStep) = 0x12;
        return;
    case 18: {
        if (B(kCounter0) != 2) return;
        if (!TakeSlot()) return;
        B(kCounter3) = B(kSlot);
        B(kStep) = 0x13;
        unsigned char* const e = Place(0x31, -0x370, 0x258, 0x3C);
        SetLong(e + 0xC, 0x580);
        return;
    }
    case 19:
        if (EffectLive(B(kCounter3))) return;
        B(kCounter3) = 0;
        B(kCounter0) = 3;
        B(kStep) = 0x14;
        return;
    case 20:
        if (B(kCounter0) != 4) return;
        if (!TakeSlot()) return;
        B(kCounter3) = B(kSlot);
        B(kStep) = 0x15;
        Place(0x13, -0x256, 0xE2, 0x3C);
        return;
    case 21: {
        if (B(kCounter0) != 6) return;
        if (EffectLive(B(kCounter3))) return;
        B(kCounter3) = 0;
        if (!TakeSlot()) return;
        B(kCounter3) = B(kSlot);
        B(kStep) = 0x16;
        unsigned char* const e = Place(0x31, -0x386, 0xE2, 0x3C);
        SetLong(e + 0xC, 0);
        return;
    }
    case 22:
        if (EffectLive(B(kCounter3))) return;
        B(kCounter3) = 0;
        B(kCounter0) = 7;
        B(kStep) = 0x1B;
        return;
    case 27: {
        if (B(kCounter0) != 0x16) return;
        unsigned char* const f = Flags();
        B(kCounter0) = 0;
        B(kCounter1) = 0;
        B(kCounter2) = 2;
        Draw_PassFlags = 0;
        B(kStep) = 0x1D;
        SH_CALL(Flags_Set)(f, 0xF);
        SH_CALL(Sound_PlayEffect)(0x201);
        ChangeArea(0x84, 0x50000, 0x460000, 0x84);
        return;
    }
    case 29:
        if (B(kCounter0) != 1) return;
        B(kCounter0) = 2;
        B(kStep) = 0x1F;
        return;
    case 31:
        if (B(kCounter0) != 3) return;
        B(kCounter0) = 4;
        B(kStep) = 0x21;
        return;
    case 33: {
        if (B(kCounter0) != 0xA) return;
        unsigned char* const f = Flags();
        B(kCounter0) = 0;
        B(kStep) = 0x22;
        SH_CALL(Flags_Set)(f, 0x10);
        ChangeArea(0x83, 0x200000, 0x2F0000, 0x87);
        return;
    }
    case 34:
        SH_CALL(Kind2_Place)(1);
        B(kStep) = 0x23;
        return;
    case 35:
        if (B(kCounter0) != 2) return;
        SH_CALL(Transition_Start)(4);
        B(kStep) = 0x24;
        return;
    case 36:
        if (MoveScript_WaitWordDA != 0) return;
        B(kCounter0) = 0;
        Draw_PassFlags = 0;
        B(kCounter2) = 2;
        B(kStep) = 0x25;
        return;
    case 37: {
        if (Field_Request == 2) return;
        unsigned char* const f = Flags();
        B(kStep) = 0x26;
        SH_CALL(Flags_Set)(f, 0x11);
        ChangeArea(0x83, 0x50000, 0x5D0000, 0x88);
        return;
    }
    case 38:
        SH_CALL(Kind2_Place)(2);
        B(kStep) = 0x27;
        return;
    case 39:
        if (B(kCounter0) != 0x15) return;
        SH_CALL(Flags_Set)(Flags(), 0x12);
        SH_CALL(ScriptFlags_Clear40)();
        EndSceneAll();
        return;
    default:
        return;
    }
}

// original 0x55E000: scene 9, steps 0, 1, 5..9. Drop-ins 9 and 10 waited on;
// flag 0x13; a transition, script message 0x3F; at step 9 the scene over,
// Field_StatusBits' 0x80 (0x56D6F0), the script flags ^ 0x16, counter 2 at 2
// and area 0x79.
SC11_EXPORT void __cdecl Scena11_Scene9(void) {
    switch (B(kStep)) {
    case 0:
        if (Field_Request == 2) return;
        B(kCounter0) = 1;
        SH_CALL(Party_DropIn)(9);
        B(kStep) = 1;
        return;
    case 1:
        if (B(kCounter0) != 7) return;
        SH_CALL(Flags_Set)(Flags(), 0x13);
        SH_CALL(ScriptFlags_Clear40)();
        EndSceneAll();
        return;
    case 5:
        if (Field_Request == 2) return;
        B(kCounter0) = 1;
        SH_CALL(Party_DropIn)(0xA);
        B(kStep) = 6;
        return;
    case 6:
        if (B(kCounter0) != 6) return;
        B(kStep) = 7;
        return;
    case 7:
        if (B(kCounter0) != 0xF) return;
        SH_CALL(Transition_Start)(4);
        B(kStep) = 8;
        return;
    case 8:
        if (MoveScript_WaitWordDA != 0) return;
        Draw_PassFlags = 0;
        SH_CALL(Msg_OpenScript)(0x3F);
        Field_Request = 2;
        B(kStep) = 9;
        return;
    case 9:
        if (Field_Request == 2) return;
        B(kStep) = 0;
        MoveScript_Var7 = 0;
        SH_AT(void (__cdecl*)(), kStatusBit80)();
        Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags ^ 0x16);
        B(kCounter2) = 2;
        ChangeArea(0x79, 0x290000, 0x330000, 7);
        return;
    default:
        return;
    }
}

// ===========================================================================
// Slot 1, the object trigger, and its handlers

// original 0x55E170: chapter 11's vtable slot 1 (PSX 0x801FA5B8), given the
// object (0x56D6D0 pushes it and then sets its +0x86 to 0xFF): handler
// Scena11_Triggers[object +0x86] with (object, the flag bits). Unchecked.
SC11_EXPORT void __cdecl Scena11_ObjectTrigger(unsigned char* object) {
    unsigned char* const f = Flags();
    const unsigned k = object[0x86];
    reinterpret_cast<void (__cdecl*)(unsigned char*, unsigned char*)>(Entry(kTriggers, static_cast<int>(k)))(object, f);
}

namespace {

// The handlers' shape: the script flags' bit 0x40, the counters 1..3 cleared,
// a step and a scene. (Stored in the original's order; nothing between.)
void StartScene(unsigned char step, signed char scene) {
    SH_CALL(ScriptFlags_Set40)();
    B(kCounter1) = 0;
    B(kCounter2) = 0;
    B(kCounter3) = 0;
    B(kStep) = step;
    MoveScript_Var7 = scene;
}

}  // namespace

// original 0x55E190: trigger 1: once (flag 5): scene 2 from step 0.
// As the original has it: the flag pointer is read again after
// ScriptFlags_Set40, and flag 5 set last.
SC11_EXPORT void __cdecl Scena11_Trigger01(void) {
    if (SH_CALL(Flags_Test)(Flags(), 5) != 0) return;
    SH_CALL(ScriptFlags_Set40)();
    unsigned char* const f = Flags();
    B(kCounter1) = 0;
    B(kCounter2) = 0;
    B(kCounter3) = 0;
    B(kStep) = 0;
    MoveScript_Var7 = 2;
    SH_CALL(Flags_Set)(f, 5);
}

// original 0x55E1E0 .. 0x55E420: triggers 2..14, each a scene from a step.
SC11_EXPORT void __cdecl Scena11_Trigger02(void) { StartScene(0, 3); }
SC11_EXPORT void __cdecl Scena11_Trigger03(void) { StartScene(0, 5); }
SC11_EXPORT void __cdecl Scena11_Trigger04(void) { StartScene(0xA, 3); }
SC11_EXPORT void __cdecl Scena11_Trigger05(void) { StartScene(0, 6); }
SC11_EXPORT void __cdecl Scena11_Trigger06(void) { StartScene(0, 7); }
SC11_EXPORT void __cdecl Scena11_Trigger07(void) { StartScene(0, 9); }
SC11_EXPORT void __cdecl Scena11_Trigger08(void) { StartScene(5, 9); }
SC11_EXPORT void __cdecl Scena11_Trigger09(void) { StartScene(0x14, 0xA); }
SC11_EXPORT void __cdecl Scena11_Trigger10(void) { StartScene(0x14, 0xB); }
// original 0x55E390: trigger 11 also clears counter 0.
SC11_EXPORT void __cdecl Scena11_Trigger11(void) {
    SH_CALL(ScriptFlags_Set40)();
    B(kStep) = 2;
    B(kCounter0) = 0;
    B(kCounter1) = 0;
    B(kCounter2) = 0;
    B(kCounter3) = 0;
    MoveScript_Var7 = 0xB;
}
SC11_EXPORT void __cdecl Scena11_Trigger12(void) { StartScene(0, 0xE); }
SC11_EXPORT void __cdecl Scena11_Trigger13(void) { StartScene(0x1E, 0xE); }
SC11_EXPORT void __cdecl Scena11_Trigger14(void) { StartScene(0x28, 5); }

// ===========================================================================
// Slots 3 and 4

// original 0x55E450: chapter 11's vtable slot 3, the arrive hook (PSX
// 0x801FAA74), answering in al. Only in area 0x79, and only at x 0x160000 or
// less (signed): with flag 0 set, scene 1 at step 0x3C; else scene 1 at step
// 0 when member 0's +8 is 0, 6 or 7, at step 0x32 otherwise; each with the
// script flags' bit 0x40, answering 1. Else 0.
SC11_EXPORT unsigned char __cdecl Scena11_ArriveHook(long x, long) {
    if (Game_AreaNumber != 0x79) return 0;
    const bool seen = SH_CALL(Flags_Test)(Flags(), 0) != 0;
    if (x > 0x160000) return 0;
    if (seen) {
        SH_CALL(ScriptFlags_Set40)();
        B(kStep) = 0x3C;
        MoveScript_Var7 = 1;
        return 1;
    }
    const unsigned char lead = B(kLeadByte8);
    SH_CALL(ScriptFlags_Set40)();
    B(kStep) = (lead == 6 || lead == 7 || lead == 0) ? 0 : 0x32;
    MoveScript_Var7 = 1;
    return 1;
}

// original 0x55E4D0: chapter 11's vtable slot 4 (PSX 0x801FAB6C): 0xFF in al.
SC11_EXPORT unsigned char __cdecl Scena11_CellHook(long, long) { return 0xFF; }

void ScenaSc11_Inject() {
    if (bof3::WantsShadow("scena_sc11")) scena_sc11::SelfTest();
    BOF3_INJECT(Scena11_Frame);
    BOF3_INJECT(Scena11_Start);
    BOF3_INJECT(Scena11_EnterArea);
    BOF3_INJECT(Scena11_Run);
    BOF3_INJECT(Scena11_Scene1);
    BOF3_INJECT(Scena11_Scene2);
    BOF3_INJECT(Scena11_Scene3);
    BOF3_INJECT(Scena11_Scene4);
    BOF3_INJECT(Scena11_EffectAnimate);
    BOF3_INJECT(Scena11_Scene5);
    BOF3_INJECT(Scena11_Scene6);
    BOF3_INJECT(Scena11_Scene8);
    BOF3_INJECT(Scena11_Scene9);
    BOF3_INJECT(Scena11_ObjectTrigger);
    BOF3_INJECT(Scena11_Trigger01);
    BOF3_INJECT(Scena11_Trigger02);
    BOF3_INJECT(Scena11_Trigger03);
    BOF3_INJECT(Scena11_Trigger04);
    BOF3_INJECT(Scena11_Trigger05);
    BOF3_INJECT(Scena11_Trigger06);
    BOF3_INJECT(Scena11_Trigger07);
    BOF3_INJECT(Scena11_Trigger08);
    BOF3_INJECT(Scena11_Trigger09);
    BOF3_INJECT(Scena11_Trigger10);
    BOF3_INJECT(Scena11_Trigger11);
    BOF3_INJECT(Scena11_Trigger12);
    BOF3_INJECT(Scena11_Trigger13);
    BOF3_INJECT(Scena11_Trigger14);
    BOF3_INJECT(Scena11_ArriveHook);
    BOF3_INJECT(Scena11_CellHook);
}

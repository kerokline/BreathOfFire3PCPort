// World 1's areas 48..52: the PSX's BIN/WORLD01/AREA048..052.EMI compiled into
// the exe at 0x408FF0..0x40AB00 (Area_Descriptors entries 48..52). Round ten,
// group AR1C: the band's 57 functions, none ours before, each read to its
// last instruction with capstone (2026-09-28) and taken through the area
// harness (area_harness.h). docs/area_w1c.md.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Reads
// by an unchecked index into the areas' .data tables are kept (they stay in
// .data); area 51's init keeps the original's header walk, which never ends
// on a zero step (docs/area_w1c.md section 6). Every call goes through the
// harness (AH_CALL / AH_AT), so the start-up fuzz can stand recorders in for
// ours as for the originals' copies; the callees of the group's own are
// called the same way, so each function is fuzzed alone.
#include "game/area_w1c.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1c_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w1c::at;
using area_harness::Mem;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(std::uint32_t address) { return *Mem(address); }
void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }
std::uint16_t High(std::int32_t v) { return static_cast<std::uint16_t>(static_cast<std::uint32_t>(v) >> 16); }
// A dword shifted right as the originals' `sar`.
std::uint32_t Sar(std::int32_t v, unsigned n) { return static_cast<std::uint32_t>(v >> n); }
// Effect_Objects record `slot` (0x80 bytes).
unsigned char* EffectAt(unsigned slot) { return Effect_Objects + slot * at::kEffectStride; }
// Party record `member` (ObjTrio, 0x14C bytes).
unsigned char* PartyAt(unsigned member) { return ObjTrio + member * at::kPartyStride; }
// The chapter's flag row (a pointer the chapters keep at 0x929ED0).
unsigned char* FlagRow() {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(at::kFlagRow)))));
}

void SetMessage(unsigned id) { SetWord(Mem(at::kMessage), id); }

// The four callees nobody owns, by their raw addresses (area_w1c_callees.h).
void FlagsToggle(unsigned char* bits, unsigned index) {
    AH_AT(void (__cdecl*)(unsigned char*, unsigned), at::kFlagsToggle)(bits, index);
}
void SetLayer(unsigned x, unsigned z, unsigned value) {
    AH_AT(void (__cdecl*)(unsigned long, unsigned long, unsigned), at::kSetLayerByte)(x, z, value);
}

// The map byte setter, as the originals pass it: x and z words, a value byte.
void SetByte(unsigned x, unsigned z, unsigned value) { AH_CALL(AreaMap_SetByte)(x, z, value); }
unsigned char ByteAt(unsigned x, unsigned z) {
    return AH_CALL(AreaMap_ByteAt)(static_cast<short>(x), static_cast<short>(z));
}

// ---- area 48 ----

// Area 48's handlers 2 and 3 (0x4090C0, 0x409170): the running object's
// direction +8 = `dir`; the script object's +7 = the count << 2 (a byte); if
// that is not 0, by MoveScript_ObjectKind: 2 MoveCmd_MoveKind2(+8),
// MoveScript_FAWord 0 and the script object's bit 0x40; else
// MoveCmd_Move(the script object, +8) and its bit 0x80. Then the running
// object's +0x14 = 0 and word +0x3E = 0x480. Sprite_Current and
// MoveScript_Object read again after every call.
void MoveScriptObject(unsigned char dir) {
    Sprite_Current[8] = dir;
    MoveScript_Object[7] = static_cast<unsigned char>(B(at::kCount48) << 2);
    if (MoveScript_Object[7] != 0) {
        if (AH_CALL(MoveScript_ObjectKind)() == 2) {
            AH_CALL(MoveCmd_MoveKind2)(Sprite_Current[8]);
            unsigned char* const object = MoveScript_Object;
            MoveScript_FAWord = 0;
            object[0] = static_cast<unsigned char>(object[0] | 0x40);
        } else {
            const unsigned char* const cur = Sprite_Current;
            AH_CALL(MoveCmd_Move)(MoveScript_Object, cur[8]);
            MoveScript_Object[0] = static_cast<unsigned char>(MoveScript_Object[0] | 0x80);
        }
    }
    SetLong(Sprite_Current + 0x14, 0);
    SetWord(Sprite_Current + 0x3E, 0x480);
}

// The count 0x92BEE7 as area 48 reads it: 2, 6, 8, or any other.
unsigned char ByCount(unsigned char two, unsigned char six, unsigned char eight, unsigned char other) {
    switch (B(at::kCount48)) {
    case 2: return two;
    case 6: return six;
    case 8: return eight;
    default: return other;
    }
}

// The tail kind 3 with sub-kind `sub` and ScriptFlags_Set40 (area 48's arrive hook).
void ArmTail3(unsigned char sub) {
    B(at::kTailKind) = 3;
    B(at::kTailSub) = sub;
    AH_CALL(ScriptFlags_Set40)();
}

// ---- area 49 ----

// Area 49's handlers 8, 9, 10, 17 (0x409980, 0x4099D0, 0x409A20, 0x409B20):
// Effect_Spawn(kind, 0, Area49_EffectKinds[party list `member`'s first byte],
// party record `member`'s words +0x2E and +0x30) with Sprite_Current made
// that record (and left so); an answer other than 0xFF stored to
// Sprite_Current[0xB], read again after the call. The member byte indexes
// the table unchecked, as the original's.
void SpawnAtMember(unsigned member, unsigned char kind) {
    const unsigned char* const record = PartyAt(member);
    const auto z = static_cast<short>(Word(record + 0x30));
    const auto x = static_cast<short>(Word(record + 0x2E));
    const unsigned char effect = B(at::kArea49EffectKinds + B(at::kPartyList0 + member));
    Sprite_Current = PartyAt(member);
    const unsigned char slot = AH_CALL(Effect_Spawn)(kind, 0, static_cast<signed char>(effect), x, z);
    if (slot != 0xFF) Sprite_Current[0xB] = slot;
}

// Area 49's handlers 19 and 20 (0x409BD0, 0x409C10): the active member's
// +0x80 bit 0 cleared; with the leader's +0x89 at 6, ScriptFlags_Set40 and
// MoveScript_Var7 = 4 with step `step`.
void RunStep(unsigned char step) {
    const unsigned char pose = B(at::kLeaderByte89);
    unsigned char* const member = Field_ActiveMember;
    member[0x80] = static_cast<unsigned char>(member[0x80] & 0xFE);
    if (pose != 6) return;
    AH_CALL(ScriptFlags_Set40)();
    MoveScript_Var7 = 4;
    B(at::kVar7Step) = step;
}

// Area 49's effect frame: a member's bit in the effect's +0xB mask and in
// Field_ScriptFlags2 cleared, if it was set (the byte and the word, each as
// the original's; bits past 7 / 15 never set).
void ClearMemberBit(unsigned char* effect, unsigned bit) {
    if ((effect[0xB] & static_cast<unsigned char>(bit)) == 0) return;
    effect[0xB] = static_cast<unsigned char>(effect[0xB] & ~bit);
    Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & ~bit);
}

// ---- area 52 ----

// Area 52 moves a block of 2 x 2 map cells: the object keeps the map bytes
// it covers in its +0x18 (row z) and +0x1C (row z + 1), the low byte column
// x, the next column x + 1; the cells it covers are marked 0x11 in the map
// and 0x10 in the second layer (0x572620), and a cell it leaves gets its
// byte back and a layer of 0.
//
// One column x of the block taken: rows z and z + 1 marked, their map bytes
// or'ed into +0x18 / +0x1C at `shift` (0 or 8). Sprite_Current read after
// each layer call, before the map read.
void StashColumn(unsigned x, unsigned z, unsigned shift) {
    SetLayer(x, z, 0x10);
    unsigned char* p = Sprite_Current + 0x18;
    unsigned b = ByteAt(x, z);
    SetLong(p, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(p)) | b << shift));
    SetByte(x, z, 0x11);
    SetLayer(x, z + 1, 0x10);
    p = Sprite_Current + 0x1C;
    b = ByteAt(x, z + 1);
    SetLong(p, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(p)) | b << shift));
    SetByte(x, z + 1, 0x11);
}
// One column x given back: its layer 0 and its bytes from +0x18 / +0x1C
// shifted right (arithmetically) by `shift`, Sprite_Current read after each
// layer call.
void RestoreColumn(unsigned x, unsigned z, unsigned shift) {
    SetLayer(x, z, 0);
    SetByte(x, z, Sar(Long(Sprite_Current + 0x18), shift));
    SetLayer(x, z + 1, 0);
    SetByte(x, z + 1, Sar(Long(Sprite_Current + 0x1C), shift));
}

// Area 52's handlers 3 and 4 (0x40A1D0, 0x40A260): Area52_BlockCell; with
// counter 3 at 0xA and the object's x not `stop`, Area52_DropColumn and the
// script position + 6; at 0x14, Area52_ShiftBlockX and, the object's x then
// 0x858000, the script position + 3 and the tail state 0x2D.
void MoveBlock(std::int32_t stop) {
    long x, z;
    if (!AH_CALL(Area52_BlockCell)(&x, &z)) return;
    const unsigned char counter = B(at::kCounter3);
    if (counter == 0xA) {
        if (Long(Sprite_Current + 0x34) == stop) return;
        AH_CALL(Area52_DropColumn)(x, z);
        AddWord(MoveScript_Object + 0xA, 6);
        return;
    }
    if (counter != 0x14) return;
    AH_CALL(Area52_ShiftBlockX)(x, z);
    if (Long(Sprite_Current + 0x34) != 0x858000) return;
    AddWord(MoveScript_Object + 0xA, 3);
    B(at::kTailState) = 0x2D;
}

// Area 52's handlers 6 and 7 (0x40A960, 0x40A9C0): Effect_FindFree; a slot
// gets z +0x38, x +0x34 (each read back), +0 = 1, kind +5 = 0x48, +0xB =
// `b`, and +0x3C the ground's elevation there (a signed word) << 8.
void SpawnEffect48(std::int32_t x, std::int32_t z, unsigned char b) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    unsigned char* const e = EffectAt(slot);
    SetLong(e + 0x38, z);
    const std::int32_t zz = Long(e + 0x38);
    SetLong(e + 0x34, x);
    const std::int32_t xx = Long(e + 0x34);
    e[0] = 1;
    e[5] = 0x48;
    const auto ground = static_cast<std::int16_t>(AH_CALL(AreaMap_Elevation)(xx, zz));
    e[0xB] = b;
    SetLong(e + 0x3C, static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(ground)) << 8));
}

// Area 52's tail: a word timer 0x9039F6 counted down, below 4 the next state.
bool CountDownBelow4() {
    const auto left = static_cast<std::uint16_t>(Word(Mem(at::kTailTimer)) - 1);
    SetWord(Mem(at::kTailTimer), left);
    return left < 4;
}
bool CountDownToZero() {
    const auto left = static_cast<std::uint16_t>(Word(Mem(at::kTailTimer)) - 1);
    SetWord(Mem(at::kTailTimer), left);
    return left == 0;
}
// A timed scene begun: counter 3 = `counter`, extra object 0 shown (its
// byte +1 = 0), the timer 0x10, state `next`.
void BeginTimed(unsigned char counter, unsigned char next) {
    B(at::kCounter3) = counter;
    B(at::kArea52ExtraObjects + 1) = 0;
    SetWord(Mem(at::kTailTimer), 0x10);
    B(at::kTailState) = next;
}
// The tail disarmed: ScriptFlags_Clear40, kind and state 0.
void Disarm() {
    AH_CALL(ScriptFlags_Clear40)();
    B(at::kTailKind) = 0;
    B(at::kTailState) = 0;
}
// A timed scene ended: story flag `flag` cleared, extra object 0 hidden (its
// byte +1 = 0xFF), the tail disarmed.
void EndTimed(unsigned flag) {
    AH_CALL(Flags_Clear)(Mem(at::kStoryFlags), flag);
    B(at::kArea52ExtraObjects + 1) = 0xFF;
    Disarm();
}

}  // namespace

// ===========================================================================
// Area 48 (descriptor 0x5F98A8): seven choices, six handlers (choices 1..6),
// the tail kind 3, the arrive hook, the init.
// ===========================================================================

// original 0x408FF0 (Area48_Choices[0]): message 0xFFFF; an answer with bit 0
// clear: counter 0 = 0xB and Sound_PlayEffect(0x205); set: counter 0 = 1,
// ScriptFlags_Clear40, MoveScript_Var7 and its step 0.
extern "C" void __cdecl Area48_ChoiceCounterBOr1(void) {
    const unsigned char answer = B(at::kChoiceAnswer);
    SetMessage(0xFFFF);
    if ((answer & 1) == 0) {
        B(at::kCounter0) = 0xB;
        AH_CALL(Sound_PlayEffect)(0x205);
        return;
    }
    B(at::kCounter0) = 1;
    AH_CALL(ScriptFlags_Clear40)();
    MoveScript_Var7 = 0;
    B(at::kVar7Step) = 0;
}

// original 0x409040 (Area48_Choices[1] = Area48_Handlers[0]; PSX 0x801F4804):
// the active member's +0x80 bit 0 cleared; with MoveScript_EffectState[the
// leader's +0x89] at 1 (the index unchecked), story flag 0xD toggled
// (0x57C160).
extern "C" void __cdecl Area48_ToggleFlagD(void) {
    Field_ActiveMember[0x80] = static_cast<unsigned char>(Field_ActiveMember[0x80] & 0xFE);
    if (B(at::kEffectState + B(at::kLeaderByte89)) != 1) return;
    FlagsToggle(Mem(at::kStoryFlags), 0xD);
}

// original 0x409080 (Area48_Choices[2] = Area48_Handlers[1]; PSX 0x801F4864):
// as Area48_ToggleFlagD's test; then the count 0x92BEE7 up by one below 8,
// and Sound_PlayEffect(0x20D).
extern "C" void __cdecl Area48_BumpCount(void) {
    Field_ActiveMember[0x80] = static_cast<unsigned char>(Field_ActiveMember[0x80] & 0xFE);
    if (B(at::kEffectState + B(at::kLeaderByte89)) != 1) return;
    const unsigned char count = B(at::kCount48);
    if (count < 8) B(at::kCount48) = static_cast<unsigned char>(count + 1);
    AH_CALL(Sound_PlayEffect)(0x20D);
}

// original 0x4090C0 (Area48_Choices[3] = Area48_Handlers[2]; PSX 0x801F48DC):
// MoveScriptObject with direction 5.
extern "C" void __cdecl Area48_MoveScriptObject5(void) { MoveScriptObject(5); }

// original 0x409170 (Area48_Choices[4] = Area48_Handlers[3]; PSX 0x801F49C8):
// MoveScriptObject with direction 1.
extern "C" void __cdecl Area48_MoveScriptObject1(void) { MoveScriptObject(1); }

// original 0x409220 (Area48_Choices[5] = Area48_Handlers[4]; PSX 0x801F4AB4):
// the active member's +0x83 by the count - 2: 6, 6: 7, 8: 8, else 9 - and its
// word +0x8A = 0xFFFE.
extern "C" void __cdecl Area48_PoseByCount(void) {
    Field_ActiveMember[0x83] = ByCount(6, 7, 8, 9);
    SetWord(Field_ActiveMember + 0x8A, 0xFFFE);
}

// original 0x4092B0 (Area48_Choices[6] = Area48_Handlers[5]; PSX 0x801F4B40):
// the count 0 and the byte 0x803490's bit 0 set.
extern "C" void __cdecl Area48_ResetCount(void) {
    const unsigned char byte = B(at::kByte803490);
    B(at::kCount48) = 0;
    B(at::kByte803490) = static_cast<unsigned char>(byte | 1);
}

// original 0x4092D0 (Field_ModeTailKinds[3], armed by Area48_ArriveHook): by
// the sub-kind 0x9039F5 - 1: Party_DropIn by the count (2: 6, 6: 7, 8: 1,
// else 8); 0: Party_DropIn(0), then the sub-kind read again; 2 (either way):
// Party_DropIn(2). Then ScriptFlags_Clear40 and the kind, state and sub-kind
// 0.
extern "C" void __cdecl Area48_TailDropIn(void) {
    unsigned char sub = B(at::kTailSub);
    if (sub == 1) {
        AH_CALL(Party_DropIn)(ByCount(6, 7, 1, 8));
    } else {
        if (sub == 0) {
            AH_CALL(Party_DropIn)(0);
            sub = B(at::kTailSub);
        }
        if (sub == 2) AH_CALL(Party_DropIn)(2);
    }
    AH_CALL(ScriptFlags_Clear40)();
    B(at::kTailKind) = 0;
    B(at::kTailState) = 0;
    B(at::kTailSub) = 0;
}

// original 0x409340 (Area_ArriveHook's case for area 48, `(x, z)` answering
// in al): three doorways, each arming the tail kind 3 (Area48_TailDropIn)
// with ScriptFlags_Set40 - sub-kind 0 for x's high word 0x17 and z's
// 0xD..0xE; 1 for z's high word 6 or z exactly 0x58000, x's 0xC..0xD; 2 for
// z's high word 0xB or z 0xA8000, x's 0x13..0x14. The words compared as
// 16-bit; every test is made. Answers 0.
extern "C" int __cdecl Area48_ArriveHook(long x, long z) {
    const std::uint16_t xh = High(x), zh = High(z);
    if (xh == 0x17 && static_cast<std::uint16_t>(zh - 0xD) < 2) ArmTail3(0);
    if ((zh == 6 || z == 0x58000) && static_cast<std::uint16_t>(xh - 0xC) < 2) ArmTail3(1);
    if ((zh == 0xB || z == 0xA8000) && static_cast<std::uint16_t>(xh - 0x13) < 2) ArmTail3(2);
    return 0;
}

// original 0x4093D0 (area 48's init, the descriptor's +0x40; PSX
// 0x801F4D2C): with Cond_ByteFD 5, Sound_PlayEffect(0x20C).
extern "C" void __cdecl Area48_InitSound(void) {
    if (Cond_ByteFD == 5) AH_CALL(Sound_PlayEffect)(0x20C);
}

// ===========================================================================
// Area 49 (descriptor 0x5FBBF8): 23 choices (five of them other groups'
// shared bodies), 21 handlers (choices 2..22), the step and cell hooks, the
// effect frame effect kind 0x70 runs here, and their helpers.
// ===========================================================================

// original 0x4093F0 (Area49_Choices[0]): the message word from
// Area49_ChoiceMessages[the answer as s8] (unchecked: a negative answer reads
// before it); answer 0 also sets flag 0x3B of the chapter's row (0x929ED0).
extern "C" void __cdecl Area49_ChoiceMessageFlag3B(void) {
    const auto answer = static_cast<signed char>(B(at::kChoiceAnswer));
    SetMessage(Word(Mem(at::kArea49ChoiceMessages + static_cast<std::uint32_t>(answer * 2))));
    if (answer != 0) return;
    AH_CALL(Flags_Set)(FlagRow(), 0x3B);
}

// original 0x409420 (Area49_Choices[1]): the message word from
// Area49_ChoiceMessages[2 + the answer as s8], unchecked.
extern "C" void __cdecl Area49_ChoiceMessage(void) {
    const auto answer = static_cast<signed char>(B(at::kChoiceAnswer));
    SetMessage(Word(Mem(at::kArea49ChoiceMessages + 4 + static_cast<std::uint32_t>(answer * 2))));
}

// original 0x409440 (Area_StepHook's case for area 49, `(x, z)` answering in
// al): a step at x exactly 0x4D8000 with Cond_ByteFD 0 runs
// Area49_SwapFlags2To3. Answers 0.
extern "C" int __cdecl Area49_StepHook(long x, long) {
    if (x == 0x4D8000 && Cond_ByteFD == 0) AH_CALL(Area49_SwapFlags2To3)();
    return 0;
}

// original 0x409460 (called by Area49_StepHook and Area49_EffectFrame):
// story flag 2 cleared, 3 set.
extern "C" void __cdecl Area49_SwapFlags2To3(void) {
    AH_CALL(Flags_Clear)(Mem(at::kStoryFlags), 2);
    AH_CALL(Flags_Set)(Mem(at::kStoryFlags), 3);
}

// original 0x409480 (called only by effect kind 0x70's handler EffectKind70_Run 0x46D780 when
// Game_AreaNumber is 0x31; Sprite_Current the effect, its +0xB a mask of
// members). With Field_Request 5 and the area not the pending one 0x937F82:
// Area49_SwapFlags2To3 and done. Otherwise Field_ScriptFlags bit 0x2000
// cleared and, for each member m (the count read before, and again after each
// member), Sprite_Current and Field_State made party record m (Field_State
// left so; Sprite_Current put back at the end) and:
//  - Cond_Flags row 9's flag 0x25, or row 5's 0x3A, or m in no zone
//    (Area49_MemberZone 0xFF): m's bit cleared in the effect's mask and in
//    Field_ScriptFlags2, if it was set;
//  - in a zone, m's bit clear: unless m's bit is set in Field_ScriptFlags2
//    or Field_ScriptFlags, Field_State +0x128 = 2, the record's +9 = 0, the
//    bit set in both masks, the direction +8 from Area49_Zones[zone]'s byte 4
//    (turned by 4 when its story flag, byte 5, is set) and
//    Sprite_EnsureAnimation(+8);
//  - in a zone, m's bit set: a record with +9 at 0 runs Field_JumpSetUp (and
//    Field_JumpCamera for m 0); one mid-step turns to the zone's direction if
//    it differs (the steps +0xC, +0x10, +0x14 negated, +9 = 8 - +9); then
//    Field_LeaderStepTick, +9 less 1, Sprite_ScriptTick and Field_ScriptFlags
//    bit 0x2000 set.
// Then, with Field_Request 5, bit 0x2000 cleared again. Sprite_Current read
// again after every call, as the original's.
extern "C" void __cdecl Area49_EffectFrame(void) {
    if (Field_Request == 5 && Game_AreaNumber != Word(Mem(at::kPendingArea))) {
        AH_CALL(Area49_SwapFlags2To3)();
        return;
    }
    const unsigned char count = Field_MemberCount;
    Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xDFFF);
    unsigned char* const effect = Sprite_Current;
    if (count != 0) {
        unsigned char m = 0;
        do {
            const unsigned bit = 1u << (m & 31);
            unsigned char* const record = PartyAt(m);
            Field_State = record;
            Sprite_Current = record;
            if (AH_CALL(Flags_Test)(Mem(at::kCondRow9), 0x25)) {
                ClearMemberBit(effect, bit);
            } else if (AH_CALL(Flags_Test)(Mem(at::kCondRow5), 0x3A)) {
                ClearMemberBit(effect, bit);
            } else {
                const unsigned char zone = AH_CALL(Area49_MemberZone)(m);
                const unsigned char* const row = Mem(at::kArea49Zones + zone * 6u);
                if (zone == 0xFF) {
                    ClearMemberBit(effect, bit);
                } else if ((effect[0xB] & static_cast<unsigned char>(bit)) == 0) {
                    if (((static_cast<unsigned>(Field_ScriptFlags2) | Field_ScriptFlags) & bit) == 0) {
                        Field_State[0x128] = 2;
                        Sprite_Current[9] = 0;
                        effect[0xB] = static_cast<unsigned char>(effect[0xB] | bit);
                        unsigned char* const cur = Sprite_Current;
                        Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 | bit);
                        cur[8] = row[4];
                        if (AH_CALL(Flags_Test)(Mem(at::kStoryFlags), row[5])) Sprite_Current[8] ^= 4;
                        AH_CALL(Sprite_EnsureAnimation)(Sprite_Current[8]);
                    }
                } else {
                    if (Sprite_Current[9] == 0) {
                        AH_CALL(Field_JumpSetUp)();
                        if (m == 0) AH_CALL(Field_JumpCamera)();
                    } else {
                        unsigned char dir = row[4];
                        if (AH_CALL(Flags_Test)(Mem(at::kStoryFlags), row[5])) dir ^= 4;
                        unsigned char* const cur = Sprite_Current;
                        if (cur[8] != dir) {
                            cur[8] = dir;
                            SetLong(cur + 0xC, -Long(cur + 0xC));
                            SetLong(cur + 0x10, -Long(cur + 0x10));
                            SetLong(cur + 0x14, -Long(cur + 0x14));
                            cur[9] = static_cast<unsigned char>(8 - cur[9]);
                        }
                    }
                    AH_CALL(Field_LeaderStepTick)();
                    Sprite_Current[9] = static_cast<unsigned char>(Sprite_Current[9] - 1);
                    AH_CALL(Sprite_ScriptTick)();
                    B(at::kScriptFlagsHigh) = static_cast<unsigned char>(B(at::kScriptFlagsHigh) | 0x20);
                }
            }
            ++m;
        } while (m < Field_MemberCount);
    }
    if (Field_Request == 5) Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xDFFF);
    Sprite_Current = effect;
}

// original 0x409760 (called by Area49_EffectFrame, `(member)`, answering in
// al): the first of Area49_Zones' seven rectangles holding party record
// member's (a byte) position +0x34 / +0x38 - x0 <= x <= x1 and z0 <= z <= z1,
// each bound the zone's byte << 16, signed compares; 0xFF for none.
extern "C" unsigned char __cdecl Area49_MemberZone(unsigned member) {
    const unsigned char* const record = PartyAt(member & 0xFF);
    const std::int32_t x = Long(record + 0x34), z = Long(record + 0x38);
    for (unsigned i = 0; i < at::kArea49ZoneCount; ++i) {
        const unsigned char* const zone = Mem(at::kArea49Zones + i * 6u);
        if (x < static_cast<std::int32_t>(zone[0] << 16) || x > static_cast<std::int32_t>(zone[2] << 16)) continue;
        if (z < static_cast<std::int32_t>(zone[1] << 16) || z > static_cast<std::int32_t>(zone[3] << 16)) continue;
        return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

// original 0x4097D0 (Area_CellHooks' entry for area 0x31, `(x, z)` answering
// in al): the first of Area49_CellSwitches' nine records whose x and z bytes
// are the cell's and whose pose is the leader's byte +8; none: 0. Story flag
// 0x1C set: 0. Else its story flag toggled (0x57C160), 0x469FE0(0xF),
// Sound_PlayEffect(0x202), answer 1.
extern "C" int __cdecl Area49_CellHook(long x, long z) {
    const unsigned char pose = B(at::kLeaderPose);
    unsigned i = 0;
    for (; i < at::kArea49CellSwitchCount; ++i) {
        const unsigned char* const r = Mem(at::kArea49CellSwitches + i * 4u);
        if (static_cast<unsigned char>(x) == r[0] && static_cast<unsigned char>(z) == r[1] && r[2] == pose) break;
    }
    if (i == at::kArea49CellSwitchCount) return 0;
    if (AH_CALL(Flags_Test)(Mem(at::kStoryFlags), 0x1C)) return 0;
    FlagsToggle(Mem(at::kStoryFlags), B(at::kArea49CellSwitches + i * 4u + 3));
    AH_AT(void (__cdecl*)(unsigned), at::kSpawnKind4)(0xF);
    AH_CALL(Sound_PlayEffect)(0x202);
    return 1;
}

// original 0x409850 (Area49_Choices[2] = Area49_Handlers[0]; PSX 0x801F3288):
// flag 0x3F of the chapter's row set.
extern "C" void __cdecl Area49_SetRowFlag3F(void) { AH_CALL(Flags_Set)(FlagRow(), 0x3F); }

// original 0x409870 (Area49_Handlers[1]; PSX 0x801F32B0): map cells x
// 0x29..0x2B of rows 0x5B and 0x5C zeroed.
extern "C" void __cdecl Area49_ClearCells(void) {
    for (unsigned z = 0x5B; z <= 0x5C; ++z)
        for (unsigned x = 0x29; x <= 0x2B; ++x) SetByte(x, z, 0);
}

// original 0x4098C0 (Area49_Handlers[2]; PSX 0x801F3328): the same cells set,
// row 0x5B to 0xC0 and row 0x5C to 0xA1.
extern "C" void __cdecl Area49_SetCells(void) {
    for (unsigned x = 0x29; x <= 0x2B; ++x) SetByte(x, 0x5B, 0xC0);
    for (unsigned x = 0x29; x <= 0x2B; ++x) SetByte(x, 0x5C, 0xA1);
}

// original 0x409920 (Area49_Handlers[3]; PSX 0x801F33A0): with the second
// party list's first byte 0x904063 at 4, counter 1 = 1.
extern "C" void __cdecl Area49_Counter1If4(void) {
    if (B(at::kPartyList0 + 1) == 4) B(at::kCounter1) = 1;
}

// original 0x409940 (Area49_Handlers[4]; PSX 0x801F33C4): with 0x904064 at 4,
// counter 2 = 1.
extern "C" void __cdecl Area49_Counter2If4(void) {
    if (B(at::kPartyList0 + 2) == 4) B(at::kCounter2) = 1;
}

// original 0x409960 (Area49_Handlers[5]; shared by areas 8, 15, 26, 49, 77,
// 80, 131; PSX 0x801F33E8 for area 49's): Camera_ShiftY + 0x14,
// MapView_Redraw 2.
extern "C" void __cdecl Area49_ShiftCameraUp(void) {
    Camera_ShiftY = static_cast<short>(Camera_ShiftY + 0x14);
    MapView_Redraw = 2;
}

// original 0x409970 (Area49_Handlers[6]; shared by areas 8, 15, 26, 49, 80,
// 131; PSX 0x801F3410): Camera_ShiftY - 0x14, MapView_Redraw 2.
extern "C" void __cdecl Area49_ShiftCameraDown(void) {
    Camera_ShiftY = static_cast<short>(Camera_ShiftY - 0x14);
    MapView_Redraw = 2;
}

// originals 0x409980, 0x4099D0, 0x409A20 (Area49_Handlers[8..10]; PSX
// 0x801F3454, 0x801F34D4, 0x801F3554): SpawnAtMember for members 0, 1, 2,
// effect kind 4.
extern "C" void __cdecl Area49_SpawnAtMember0(void) { SpawnAtMember(0, 4); }
extern "C" void __cdecl Area49_SpawnAtMember1(void) { SpawnAtMember(1, 4); }
extern "C" void __cdecl Area49_SpawnAtMember2(void) { SpawnAtMember(2, 4); }

// original 0x409A70 (Area49_Handlers[11]; PSX 0x801F35D4): map cells (0xB,
// 0x55) and (0xC, 0x55) zeroed.
extern "C" void __cdecl Area49_ClearCellsB(void) {
    SetByte(0xB, 0x55, 0);
    SetByte(0xC, 0x55, 0);
}

// original 0x409A90 (Area49_Handlers[12]; PSX 0x801F360C): with the leader's
// +0x89 at 5, an effect of kind 0x6C (Effect_FindFree; none: skipped),
// Sound_PlayEffect(0x209), the active member's +0x80 bit 0 cleared (the
// pointer read after the sound), cells (0xB, 0x55) and (0xC, 0x55) set to
// 0xC0, counter 0 = 0 and MoveScript_Var7 = 4 with step 0x1E. Otherwise only
// the active member's bit.
extern "C" void __cdecl Area49_SpawnEffect6C(void) {
    if (B(at::kLeaderByte89) != 5) {
        Field_ActiveMember[0x80] = static_cast<unsigned char>(Field_ActiveMember[0x80] & 0xFE);
        return;
    }
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot != 0xFF) {
        unsigned char* const e = EffectAt(slot);
        e[0] = 1;
        e[5] = 0x6C;
    }
    AH_CALL(Sound_PlayEffect)(0x209);
    Field_ActiveMember[0x80] = static_cast<unsigned char>(Field_ActiveMember[0x80] & 0xFE);
    SetByte(0xB, 0x55, 0xC0);
    SetByte(0xC, 0x55, 0xC0);
    B(at::kCounter0) = 0;
    MoveScript_Var7 = 4;
    B(at::kVar7Step) = 0x1E;
}

// original 0x409B20 (Area49_Handlers[17]; PSX 0x801F376C): SpawnAtMember for
// member 2, effect kind 3.
extern "C" void __cdecl Area49_SpawnKind3AtMember2(void) { SpawnAtMember(2, 3); }

// original 0x409B70 (Area49_Handlers[18]; PSX 0x801F37EC): map cells x 0x4D
// and 0x4E of rows 0x22..0x25 zeroed.
extern "C" void __cdecl Area49_ClearCellsC(void) {
    for (unsigned x = 0x4D; x <= 0x4E; ++x)
        for (unsigned z = 0x22; z <= 0x25; ++z) SetByte(x, z, 0);
}

// originals 0x409BD0, 0x409C10 (Area49_Handlers[19], [20]; PSX 0x801F3884,
// 0x801F3908): RunStep with step 5, 6.
extern "C" void __cdecl Area49_RunStep5(void) { RunStep(5); }
extern "C" void __cdecl Area49_RunStep6(void) { RunStep(6); }

// ===========================================================================
// Area 50 (descriptor 0x5FCD48): a choice, a handler, an object trigger (its
// other choices are other groups' shared bodies).
// ===========================================================================

// original 0x409C50 (Area50_Choices[0]): answer not 0: message 0x84 and the
// mark 0x9398CF = 6; answer 0: the byte 0x9045FB at 0x1E or more message
// 0x82, below message 0x83 and the mark 6.
extern "C" void __cdecl Area50_ChoiceAsk82(void) {
    if (B(at::kChoiceAnswer) != 0) {
        SetMessage(0x84);
        B(at::kAnswerMark) = 6;
        return;
    }
    if (B(at::kByte9045FB) >= 0x1E) {
        SetMessage(0x82);
        return;
    }
    SetMessage(0x83);
    B(at::kAnswerMark) = 6;
}

// original 0x409C90 (Area50_Handlers[0]; PSX 0x801F2D48): map cells x 8 of
// rows 0x25..0x28 set to 0x10.
extern "C" void __cdecl Area50_SetCells(void) {
    for (unsigned z = 0x25; z <= 0x28; ++z) SetByte(8, z, 0x10);
}

// original 0x409CC0 (Field_ObjectTriggers id 43, (object, flags) ignored):
// ScriptFlags_Set40, the mode tail kind 4 with sub-kind 4 (a register store,
// mov al 4: the tool's immediate scan does not see it); answers 0 in al.
extern "C" unsigned char __cdecl Area50_Trigger43(unsigned char*, unsigned char*) {
    AH_CALL(ScriptFlags_Set40)();
    B(at::kTailKind) = 4;
    B(at::kTailSub) = 4;
    return 0;
}

// ===========================================================================
// Area 51 (descriptor 0x5FD450): three choices, two handlers (choices 1, 2),
// the init.
// ===========================================================================

// original 0x409CE0 (Area51_Choices[1] = Area51_Handlers[0]; PSX 0x801F2C04):
// Party_MemberAt(the running object's +0x34, +0x38, +0x70); a member (not
// 0xFF), with Field_ScriptFlags2 bit 0x1000 clear and Field_Request 0:
// 0x57C8A0(member, 1).
extern "C" void __cdecl Area51_MemberAtObject(void) {
    const unsigned char* const cur = Sprite_Current;
    const unsigned char member =
        AH_CALL(Party_MemberAt)(Long(cur + 0x34), Long(cur + 0x38), static_cast<unsigned>(Long(cur + 0x70)));
    if (member == 0xFF) return;
    if (Field_ScriptFlags2 & 0x1000) return;
    if (Field_Request != 0) return;
    AH_AT(void (__cdecl*)(unsigned, unsigned), at::kMemberSetState)(member, 1);
}

// original 0x409D30 (Area51_Choices[2] = Area51_Handlers[1]; PSX 0x801F2C7C):
// the running object put at the position (+0x34, +0x38) of the object ten
// records (0x668 bytes) before the active member, and its word +0x3E the
// ground's elevation there less 0x40. Sprite_Current read again for each
// store, as the original's.
extern "C" void __cdecl Area51_PlaceAtMember(void) {
    const unsigned char* const from = Field_ActiveMember - 0x668;
    SetLong(Sprite_Current + 0x34, Long(from + 0x34));
    SetLong(Sprite_Current + 0x38, Long(from + 0x38));
    const unsigned char* const cur = Sprite_Current;
    const std::int32_t ground = AH_CALL(AreaMap_Elevation)(Long(cur + 0x34), Long(cur + 0x38));
    SetWord(Sprite_Current + 0x3E, static_cast<unsigned>(ground - 0x40));
}

// original 0x409D80 (Area51_Choices[0]): message 1 for an answer not 0, 0x14
// for 0.
extern "C" void __cdecl Area51_ChoiceMessage(void) { SetMessage(B(at::kChoiceAnswer) != 0 ? 1 : 0x14); }

// original 0x409DA0 (area 51's init, the descriptor's +0x40; PSX
// 0x801F2D00): with Cond_Flags row 14's flag 0x13 set and 0x14 clear, the
// first header entry of kind 0x81 from AreaMap_EntryBase gets both colour
// dwords, +0xC then +8, 0x04214E73 (Area11_DimBackdrop's walk with its own
// colour). A zero dword ends the walk, the byte +2 is the step in dwords; as
// the original, a step of 0 on an entry of another kind never ends (read,
// not run: the fuzz keeps the steps above 0).
extern "C" void __cdecl Area51_TintBackdrop(void) {
    if (!AH_CALL(Flags_Test)(Mem(at::kCondRow14), 0x13)) return;
    if (AH_CALL(Flags_Test)(Mem(at::kCondRow14), 0x14)) return;
    unsigned char* entry = AreaMap_Header + static_cast<std::uint32_t>(AreaMap_EntryBase) * 4u;
    for (auto e = static_cast<std::uint32_t>(Long(entry)); e != 0; e = static_cast<std::uint32_t>(Long(entry))) {
        if ((e & 0xFF000000u) == 0x81000000u) {
            SetLong(entry + 12, 0x04214E73);
            SetLong(entry + 8, 0x04214E73);
            return;
        }
        entry += ((e >> 16) & 0xFFu) * 4u;
    }
}

// ===========================================================================
// Area 52 (descriptor 0x5FE080): nine handlers, the cell hook, the tail kind
// 15, and the block helpers (docs/area_w1c.md section 1).
// ===========================================================================

// original 0x409E20 (Area52_Handlers[0]; PSX 0x801F3280): with Field_Request
// 5, the script position back 2 and done. Else the running object's +0x18 and
// +0x1C cleared and the block under it taken: columns x and x + 1 (x, z the
// high words of its position less 0x8000) by StashColumn.
extern "C" void __cdecl Area52_StashBlock(void) {
    if (Field_Request == 5) {
        AddWord(MoveScript_Object + 0xA, -2);
        return;
    }
    unsigned char* const cur = Sprite_Current;
    const std::uint16_t z = High(static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(cur + 0x38)) - 0x8000u));
    const std::uint16_t x = High(static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(cur + 0x34)) - 0x8000u));
    SetLong(cur + 0x18, 0);
    SetLong(Sprite_Current + 0x1C, 0);
    for (unsigned i = 0; i < 2; ++i) StashColumn(static_cast<std::uint16_t>(x + i), z, i * 8);
}

// original 0x409F20 (Area52_Handlers[1]; PSX 0x801F3414): Area52_BlockCell;
// if it answers, the block moves one row on: row z given back from +0x18
// (columns x, x + 1), +0x18 = +0x1C, +0x1C = 0, row z + 2 taken into +0x1C;
// the object's direction 5 and MoveCmd_Move(the script object, 5), its +7 =
// 2.
extern "C" void __cdecl Area52_ShiftBlockZ(void) {
    long xv, zv;
    if (!AH_CALL(Area52_BlockCell)(&xv, &zv)) return;
    std::uint16_t x = High(xv), z = High(zv);
    SetLayer(x, z, 0);
    SetByte(x, z, Sprite_Current[0x18]);
    SetLayer(static_cast<std::uint16_t>(x + 1), z, 0);
    SetByte(static_cast<std::uint16_t>(x + 1), z, Sar(Long(Sprite_Current + 0x18), 8));
    unsigned char* const cur = Sprite_Current;
    SetLong(cur + 0x18, Long(cur + 0x1C));
    SetLong(Sprite_Current + 0x1C, 0);
    z = static_cast<std::uint16_t>(z + 2);
    SetLayer(x, z, 0x10);
    unsigned char* p = Sprite_Current + 0x1C;
    unsigned b = ByteAt(x, z);
    SetLong(p, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(p)) | b));
    SetByte(x, z, 0x11);
    x = static_cast<std::uint16_t>(x + 1);
    SetLayer(x, z, 0x10);
    p = Sprite_Current + 0x1C;
    b = ByteAt(x, z);
    SetLong(p, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(p)) | b << 8));
    SetByte(x, z, 0x11);
    Sprite_Current[8] = 5;
    AH_CALL(MoveCmd_Move)(MoveScript_Object, 5);
    MoveScript_Object[7] = 2;
}

// original 0x40A070 (Area52_Handlers[2]; PSX 0x801F35A0): with the running
// object exactly at (0x118000, 0x298000), the block goes back to its start:
// columns 0x11, 0x12 of rows 0x29, 0x2A given back (RestoreColumn), +0x18 and
// +0x1C cleared, z +0x38 = 0x78000, and the block at columns 0x11, 0x12 of
// the new z (high word of z less 0x8000) taken (StashColumn).
extern "C" void __cdecl Area52_ResetBlock(void) {
    const unsigned char* const cur = Sprite_Current;
    if (Long(cur + 0x34) != 0x118000 || Long(cur + 0x38) != 0x298000) return;
    for (unsigned i = 0; i < 2; ++i) RestoreColumn(0x11 + i, 0x29, i * 8);
    SetLong(Sprite_Current + 0x18, 0);
    SetLong(Sprite_Current + 0x1C, 0);
    SetLong(Sprite_Current + 0x38, 0x78000);
    const std::uint16_t z = High(static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Sprite_Current + 0x38)) - 0x8000u));
    for (unsigned i = 0; i < 2; ++i) StashColumn(0x11 + i, z, i * 8);
}

// originals 0x40A1D0 / 0x40A260 (Area52_Handlers[3], [4]; PSX 0x801F37A8,
// 0x801F3894): MoveBlock stopping at x 0x688000 / 0x668000.
extern "C" void __cdecl Area52_MoveBlockA(void) { MoveBlock(0x688000); }
extern "C" void __cdecl Area52_MoveBlockB(void) { MoveBlock(0x668000); }

// original 0x40A2F0 (called by area 52's handlers 1, 3, 4, 8, `(x, z)`
// pointers to the caller's two dwords, answering in al): *x = the running
// object's +0x34 less 0x8000, *z = its +0x38 less 0x8000; with
// Field_Request 5, Area52_RestoreBlock(*x, z). Counter 3 at 0: the script
// position back 2 and 0; else 1.
extern "C" unsigned char __cdecl Area52_BlockCell(long* x, long* z) {
    *x = static_cast<long>(static_cast<std::uint32_t>(Long(Sprite_Current + 0x34)) - 0x8000u);
    const auto zv = static_cast<long>(static_cast<std::uint32_t>(Long(Sprite_Current + 0x38)) - 0x8000u);
    *z = zv;
    if (Field_Request == 5) AH_CALL(Area52_RestoreBlock)(*x, zv);
    if (B(at::kCounter3) != 0) return 1;
    AddWord(MoveScript_Object + 0xA, -2);
    return 0;
}

// original 0x40A350 (called by Area52_MoveBlockA / B, `(x, z)` the cell's
// dwords, their high words read): the block one column on in x: column x
// given back from the low bytes of +0x18 / +0x1C, both shifted right by 8
// (arithmetically), column x + 2 taken into their high bytes; the object's
// direction 3 and MoveCmd_Move(the script object, 3), its +7 = 2.
extern "C" void __cdecl Area52_ShiftBlockX(long xv, long zv) {
    const std::uint16_t x = High(xv), z = High(zv);
    const auto z1 = static_cast<std::uint16_t>(z + 1);
    SetLayer(x, z, 0);
    SetByte(x, z, Sprite_Current[0x18]);
    SetLayer(x, z1, 0);
    SetByte(x, z1, Sprite_Current[0x1C]);
    unsigned char* cur = Sprite_Current;
    SetLong(cur + 0x18, static_cast<std::int32_t>(Sar(Long(cur + 0x18), 8)));
    cur = Sprite_Current;
    SetLong(cur + 0x1C, static_cast<std::int32_t>(Sar(Long(cur + 0x1C), 8)));
    const auto x2 = static_cast<std::uint16_t>(x + 2);
    SetLayer(x2, z, 0x10);
    unsigned char* p = Sprite_Current + 0x18;
    unsigned b = ByteAt(x2, z);
    SetLong(p, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(p)) | b << 8));
    SetByte(x2, z, 0x11);
    SetLayer(x2, z1, 0x10);
    p = Sprite_Current + 0x1C;
    b = ByteAt(x2, z1);
    SetLong(p, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(p)) | b << 8));
    SetByte(x2, z1, 0x11);
    Sprite_Current[8] = 3;
    AH_CALL(MoveCmd_Move)(MoveScript_Object, 3);
    MoveScript_Object[7] = 2;
}

// original 0x40A450 (called by Area52_MoveBlockA / B, `(x, z)`, their high
// words read): column x + 1 given back from the high bytes of +0x18 / +0x1C
// (RestoreColumn at 8), then both shifted left by 8.
extern "C" void __cdecl Area52_DropColumn(long xv, long zv) {
    RestoreColumn(static_cast<std::uint16_t>(High(xv) + 1), High(zv), 8);
    SetLong(Sprite_Current + 0x18, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Sprite_Current + 0x18)) << 8));
    SetLong(Sprite_Current + 0x1C, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Sprite_Current + 0x1C)) << 8));
}

// original 0x40A4C0 (called by Area52_BlockCell, `(x, z)`, their high words
// read): the whole block given back, columns x and x + 1 (RestoreColumn at 0
// and 8).
extern "C" void __cdecl Area52_RestoreBlock(long xv, long zv) {
    for (unsigned i = 0; i < 2; ++i) RestoreColumn(static_cast<std::uint16_t>(High(xv) + i), High(zv), i * 8);
}

// original 0x40A530 (Area_CellHooks' entry for area 0x34, `(x, z)` answering
// in al): the first of Area52_CellSwitches' four records whose x and z bytes
// are the cell's and whose low nibble of byte 2 is the leader's byte +8;
// none: 0. Else ScriptFlags_Set40, the mode tail kind 15 (Area52_TailBlock)
// at the record's state; Area52_PartyInRect answering: state 0xA, answer 1;
// else the record's story flag toggled (0x57C160), Sound_PlayEffect(0x202),
// answer 1.
extern "C" int __cdecl Area52_CellHook(long x, long z) {
    const unsigned char pose = B(at::kLeaderPose);
    unsigned i = 0;
    for (; i < at::kArea52CellSwitchCount; ++i) {
        const unsigned char* const r = Mem(at::kArea52CellSwitches + i * 5u);
        if (static_cast<unsigned char>(x) == r[0] && static_cast<unsigned char>(z) == r[1] && (r[2] & 0xF) == pose) break;
    }
    if (i == at::kArea52CellSwitchCount) return 0;
    AH_CALL(ScriptFlags_Set40)();
    const unsigned char* const r = Mem(at::kArea52CellSwitches + i * 5u);
    B(at::kTailKind) = 0xF;
    B(at::kTailState) = r[4];
    if (AH_CALL(Area52_PartyInRect)()) {
        B(at::kTailState) = 0xA;
        return 1;
    }
    FlagsToggle(Mem(at::kStoryFlags), r[3]);
    AH_CALL(Sound_PlayEffect)(0x202);
    return 1;
}

// original 0x40A5C0 (called by Area52_CellHook, answering in al): 1 when a
// member (below Field_MemberCount, read once) stands in Area52_Rects[the
// tail state not 0]: its x high word +0x36 less x0 below the width and its z
// high word +0x3A less z0 below the depth, as 16-bit unsigned compares; else
// 0.
extern "C" unsigned char __cdecl Area52_PartyInRect(void) {
    const bool armed = B(at::kTailState) != 0;
    const unsigned char count = Field_MemberCount;
    if (count == 0) return 0;
    const unsigned char* const rect = Mem(at::kArea52Rects + (armed ? 4u : 0u));
    for (unsigned char m = 0; m < count; ++m) {
        const unsigned char* const record = PartyAt(m);
        if (static_cast<std::uint16_t>(Word(record + 0x36) - rect[0]) >= rect[2]) continue;
        if (static_cast<std::uint16_t>(Word(record + 0x3A) - rect[1]) < rect[3]) return 1;
    }
    return 0;
}

// original 0x40A670 (Field_ModeTailKinds[15], armed by Area52_CellHook): by
// the s8 state 0x9039F4 through a byte table and a 15-entry jump table in
// the body (0..0x2F; any other returns). Three timed scenes - states 0, 0x14,
// 0x28: counter 3 = 1 / 0xA / 0x14, extra object 0 shown, a 0x10 timer; then
// counter 3 = 0 once the timer is below 4; then at 0 story flag 0x24 / 0x26 /
// 0x25 cleared, extra object 0 hidden, the tail disarmed. 0xA:
// Msg_OpenScript(0x39) and Field_Request 2; 0xB: once Field_Request leaves 2,
// disarmed. 0x2D: counter 3 = 0; 0x2E: once counter 3 is 0x1E, 0x40A940 (area
// 52's second effect); 0x2F: once it is 0x28, columns 0x86 / 0x87 of rows
// 0x11, 0x12 given layer 0 and map bytes 0 / 0x10, the timer 0xA and state
// 0x29.
extern "C" void __cdecl Area52_TailBlock(void) {
    const auto state = static_cast<signed char>(B(at::kTailState));
    switch (state) {
    case 0: BeginTimed(1, 1); return;
    case 1:
        if (!CountDownBelow4()) return;
        B(at::kCounter3) = 0;
        B(at::kTailState) = 2;
        return;
    case 2:
        if (CountDownToZero()) EndTimed(0x24);
        return;
    case 0xA:
        AH_CALL(Msg_OpenScript)(0x39);
        Field_Request = 2;
        B(at::kTailState) = 0xB;
        return;
    case 0xB:
        if (Field_Request == 2) return;
        Disarm();
        return;
    case 0x14: BeginTimed(0xA, 0x15); return;
    case 0x15:
        if (!CountDownBelow4()) return;
        B(at::kCounter3) = 0;
        B(at::kTailState) = 0x16;
        return;
    case 0x16:
        if (CountDownToZero()) EndTimed(0x26);
        return;
    case 0x28: BeginTimed(0x14, 0x29); return;
    case 0x29:
        if (!CountDownBelow4()) return;
        B(at::kCounter3) = 0;
        B(at::kTailState) = 0x2A;
        return;
    case 0x2A:
        if (CountDownToZero()) EndTimed(0x25);
        return;
    case 0x2D:
        B(at::kCounter3) = 0;
        B(at::kTailState) = 0x2E;
        return;
    case 0x2E:
        if (B(at::kCounter3) != 0x1E) return;
        AH_CALL(Area52_SpawnEffect48BJump)();
        B(at::kTailState) = 0x2F;
        return;
    case 0x2F:
        if (B(at::kCounter3) != 0x28) return;
        SetLayer(0x86, 0x11, 0);
        SetByte(0x86, 0x11, 0);
        SetLayer(0x86, 0x12, 0);
        SetByte(0x86, 0x12, 0);
        SetLayer(0x87, 0x11, 0);
        SetByte(0x87, 0x11, 0x10);
        SetLayer(0x87, 0x12, 0);
        SetByte(0x87, 0x12, 0x10);
        SetWord(Mem(at::kTailTimer), 0xA);
        B(at::kTailState) = 0x29;
        return;
    default: return;   // 3..9, 0xC..0x13, 0x17..0x27, 0x2B, 0x2C: the table's return; above 0x2F or negative: `ja` to it
    }
}

// original 0x40A940 (called by Area52_TailBlock's state 0x2E): a 5-byte
// `jmp 0x40A9C0` (Area52_SpawnEffect48B), a start of its own.
extern "C" void __cdecl Area52_SpawnEffect48BJump(void) { AH_CALL(Area52_SpawnEffect48B)(); }

// original 0x40A950 (Area52_Handlers[5]; PSX 0x801F4268): story flag 0x25
// cleared.
extern "C" void __cdecl Area52_ClearFlag25(void) { AH_CALL(Flags_Clear)(Mem(at::kStoryFlags), 0x25); }

// original 0x40A960 (Area52_Handlers[6]; PSX 0x801F4290): SpawnEffect48 at
// (0x880000, 0x110000), +0xB = 8.
extern "C" void __cdecl Area52_SpawnEffect48A(void) { SpawnEffect48(0x880000, 0x110000, 8); }

// original 0x40A9C0 (Area52_Handlers[7]; PSX 0x801F431C): SpawnEffect48 at
// (0x890000, 0x118000), +0xB = 0x10.
extern "C" void __cdecl Area52_SpawnEffect48B(void) { SpawnEffect48(0x890000, 0x118000, 0x10); }

// original 0x40AA20 (Area52_Handlers[8]; PSX 0x801F43B0): Area52_BlockCell
// (its answer not read); column x - 1 of rows z and z + 1 taken into the low
// bytes of +0x18 / +0x1C (or'ed, not shifted: Area52_DropColumn made room);
// the object's direction 7 and MoveCmd_Move(the script object, 7), its +7 =
// 2.
extern "C" void __cdecl Area52_ShiftBlockXBack(void) {
    long xv, zv;
    AH_CALL(Area52_BlockCell)(&xv, &zv);
    const auto x = static_cast<std::uint16_t>(High(xv) - 1);
    const std::uint16_t z = High(zv);
    const auto z1 = static_cast<std::uint16_t>(z + 1);
    SetLayer(x, z, 0x10);
    unsigned char* p = Sprite_Current + 0x18;
    unsigned b = ByteAt(x, z);
    SetLong(p, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(p)) | b));
    SetByte(x, z, 0x11);
    SetLayer(x, z1, 0x10);
    p = Sprite_Current + 0x1C;
    b = ByteAt(x, z1);
    SetLong(p, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(p)) | b));
    SetByte(x, z1, 0x11);
    Sprite_Current[8] = 7;
    AH_CALL(MoveCmd_Move)(MoveScript_Object, 7);
    MoveScript_Object[7] = 2;
}

void AreaW1c_Inject() {
    if (bof3::WantsShadow("area_w1c")) area_w1c::SelfTest();
    BOF3_INJECT(Area48_ChoiceCounterBOr1);
    BOF3_INJECT(Area48_ToggleFlagD);
    BOF3_INJECT(Area48_BumpCount);
    BOF3_INJECT(Area48_MoveScriptObject5);
    BOF3_INJECT(Area48_MoveScriptObject1);
    BOF3_INJECT(Area48_PoseByCount);
    BOF3_INJECT(Area48_ResetCount);
    BOF3_INJECT(Area48_TailDropIn);
    BOF3_INJECT(Area48_ArriveHook);
    BOF3_INJECT(Area48_InitSound);
    BOF3_INJECT(Area49_ChoiceMessageFlag3B);
    BOF3_INJECT(Area49_ChoiceMessage);
    BOF3_INJECT(Area49_StepHook);
    BOF3_INJECT(Area49_SwapFlags2To3);
    BOF3_INJECT(Area49_EffectFrame);
    BOF3_INJECT(Area49_MemberZone);
    BOF3_INJECT(Area49_CellHook);
    BOF3_INJECT(Area49_SetRowFlag3F);
    BOF3_INJECT(Area49_ClearCells);
    BOF3_INJECT(Area49_SetCells);
    BOF3_INJECT(Area49_Counter1If4);
    BOF3_INJECT(Area49_Counter2If4);
    BOF3_INJECT(Area49_ShiftCameraUp);
    BOF3_INJECT(Area49_ShiftCameraDown);
    BOF3_INJECT(Area49_SpawnAtMember0);
    BOF3_INJECT(Area49_SpawnAtMember1);
    BOF3_INJECT(Area49_SpawnAtMember2);
    BOF3_INJECT(Area49_ClearCellsB);
    BOF3_INJECT(Area49_SpawnEffect6C);
    BOF3_INJECT(Area49_SpawnKind3AtMember2);
    BOF3_INJECT(Area49_ClearCellsC);
    BOF3_INJECT(Area49_RunStep5);
    BOF3_INJECT(Area49_RunStep6);
    BOF3_INJECT(Area50_ChoiceAsk82);
    BOF3_INJECT(Area50_SetCells);
    BOF3_INJECT(Area50_Trigger43);
    BOF3_INJECT(Area51_MemberAtObject);
    BOF3_INJECT(Area51_PlaceAtMember);
    BOF3_INJECT(Area51_ChoiceMessage);
    BOF3_INJECT(Area51_TintBackdrop);
    BOF3_INJECT(Area52_StashBlock);
    BOF3_INJECT(Area52_ShiftBlockZ);
    BOF3_INJECT(Area52_ResetBlock);
    BOF3_INJECT(Area52_MoveBlockA);
    BOF3_INJECT(Area52_MoveBlockB);
    BOF3_INJECT(Area52_BlockCell);
    BOF3_INJECT(Area52_ShiftBlockX);
    BOF3_INJECT(Area52_DropColumn);
    BOF3_INJECT(Area52_RestoreBlock);
    BOF3_INJECT(Area52_CellHook);
    BOF3_INJECT(Area52_PartyInRect);
    BOF3_INJECT(Area52_TailBlock);
    BOF3_INJECT(Area52_SpawnEffect48BJump);
    BOF3_INJECT(Area52_ClearFlag25);
    BOF3_INJECT(Area52_SpawnEffect48A);
    BOF3_INJECT(Area52_SpawnEffect48B);
    BOF3_INJECT(Area52_ShiftBlockXBack);
}

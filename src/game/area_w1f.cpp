// World 1's areas 68..69 and 71..75: the PSX's BIN/WORLD01/AREA068..075.EMI
// compiled into the exe at 0x40CEF0..0x40EB90 (Area_Descriptors entries 68..75;
// area 70 has no code). Round ten, group AR1F: the band's 59 functions, none
// ours before, each read to its last instruction with capstone (2026-09-28) and
// taken through the area harness (area_harness.h). docs/area_w1f.md.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Reads
// by an unchecked index into the areas' .data tables are kept (they stay in
// .data); where the original jumps or calls through a pointer read past a
// table (the running object's state byte, area 75's phase and list bytes) ours
// aborts with a message instead of faulting (round nine section 6: no ledger
// entry). Every call goes through the harness (AH_CALL / AH_AT), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies;
// the group's own callees are called the same way, so each function is
// fuzzed alone.
#include "game/area_w1f.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1f_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w1f::at;
using area_harness::Mem;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(std::uint32_t address) { return *Mem(address); }
void AddWord(unsigned char* p, unsigned v) { SetWord(p, Word(p) + v); }
void SetMessage(unsigned id) { SetWord(Mem(at::kMessage), id); }
// Field object `index` (Sprite_Objects, 0xA4 bytes); effect record `slot`
// (Effect_Objects, 0x80 bytes); party record `member` (ObjTrio, 0x14C bytes).
unsigned char* ObjectAt(unsigned index) { return Sprite_Objects + index * at::kObjectStride; }
unsigned char* EffectAt(unsigned slot) { return Effect_Objects + slot * at::kEffectStride; }
unsigned char* PartyAt(unsigned member) { return ObjTrio + member * at::kPartyStride; }
// A record's index among the field objects as the originals compute it: the
// pointer less Sprite_Objects, divided by 0xA4 as a signed dword (imul by
// 0x63E7063F, sar 6, the sign added back): C's truncating division.
unsigned char ObjectIndex(const unsigned char* record) {
    const auto delta = static_cast<std::int32_t>(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(record)) -
                                                 at::kSpriteObjects);
    return static_cast<unsigned char>(delta / static_cast<std::int32_t>(at::kObjectStride));
}
// A float stored as the originals' fst does.
void SetFloat(unsigned char* p, float v) { std::memcpy(p + 0, &v, sizeof v); }

// A two-entry .data state table read in place by the running object's +4 and
// jumped through (area 68's handlers 2 and 3, 69's 0, 75's 7 and 11). The
// original indexes it unchecked: a state of 2 or more jumps through the bytes
// after the table - ours aborts there.
void RunState(const char* who, std::uint32_t table) {
    const unsigned state = Sprite_Current[4];
    if (state >= 2)
        bof3::Fatal("%s: the running object's state %u is past its 2-entry table 0x%X (the original jumps through the "
                    "bytes after it)",
                    who, state, static_cast<unsigned>(table));
    reinterpret_cast<area_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + state * 4)))))();
}

// ---- area 68 ----

// Area 68's handlers 0, 1, 4, 5, 7, 8 (0x40CFD0 ...): Effect_Spawn(kind, 0,
// kinds[party list `member`'s first byte], party record `member`'s words +0x2E
// and +0x30) with Sprite_Current made that record (and left so); an answer
// other than 0xFF stored to Sprite_Current[0xB], read again after the call.
// The list byte indexes the table unchecked (it stays in .data).
void SpawnAtMember(unsigned member, unsigned char kind, std::uint32_t kinds) {
    const unsigned char* const record = PartyAt(member);
    const auto z = static_cast<short>(Word(record + 0x30));
    const auto x = static_cast<short>(Word(record + 0x2E));
    const unsigned char effect = B(kinds + B(at::kPartyList0 + member));
    Sprite_Current = PartyAt(member);
    const unsigned char slot = AH_CALL(Effect_Spawn)(kind, 0, static_cast<signed char>(effect), x, z);
    if (slot != 0xFF) Sprite_Current[0xB] = slot;
}

// The glides' state 0 (0x40D090, 0x40D380): the running object's count +0xA =
// `count`, its state 1, and the leader's script word +0x12E (through
// Field_State) back by 2.
void GlideBegin(unsigned char count) {
    Sprite_Current[0xA] = count;
    Sprite_Current[4] = 1;
    AddWord(Field_State + 0x12E, 0xFFFE);
}
// The glides' state 1 (0x40D0C0, 0x40D150, 0x40D3B0): with the count +0xA at 0
// the state goes back to 0; else x +0x34 moves by steps[count & 0xF] << 11 and
// z +0x38 by -steps[count & 0xF] << 11 (signed bytes), the count goes down by
// one and the script word +0x12E back by 2.
void GlideStep(std::uint32_t steps) {
    unsigned char* const cur = Sprite_Current;
    const unsigned char count = cur[0xA];
    if (count == 0) {
        cur[4] = 0;
        return;
    }
    const auto dx = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<signed char>(B(steps + (count & 0xF)))));
    SetLong(cur + 0x34, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(cur + 0x34)) + (dx << 11)));
    const auto dz = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<signed char>(B(steps + (cur[0xA] & 0xF)))));
    SetLong(cur + 0x38, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(cur + 0x38)) + ((0u - dz) << 11)));
    cur[0xA] = static_cast<unsigned char>(cur[0xA] - 1);
    AddWord(Field_State + 0x12E, 0xFFFE);
}

// ---- areas 72 and 73 ----

// Areas 72 and 73's inits (0x40D4B0, 0x40D580, one body over two tables): a
// weighted draw - Rand() & 0x3F walked down the eight weights, the first it
// falls below chosen (none: 8); field objects 0..7 but the chosen one get +0
// = 0; the chosen one is put at one of the eight cells (Rand() & 7: x and z
// bytes << 16), its word +0x3E the ground there; Field_EdgeBits = the
// leader's zone counter less 5 (either way).
void PlaceRandomObject(std::uint32_t cells, std::uint32_t weights) {
    auto roll = static_cast<unsigned char>(AH_CALL(Rand)() & 0x3F);
    unsigned chosen = 0;
    for (; chosen < 8; ++chosen) {
        const unsigned char weight = B(weights + chosen);
        if (roll < weight) break;
        roll = static_cast<unsigned char>(roll - weight);
    }
    for (unsigned k = 0; k < 8; ++k)
        if (k != chosen) ObjectAt(k)[0] = 0;
    if (chosen < 8) {
        const unsigned pick = static_cast<unsigned char>(AH_CALL(Rand)() & 7);
        unsigned char* const object = ObjectAt(chosen);
        const auto x = static_cast<std::int32_t>(static_cast<std::uint32_t>(B(cells + pick * 2)) << 16);
        const auto z = static_cast<std::int32_t>(static_cast<std::uint32_t>(B(cells + pick * 2 + 1)) << 16);
        SetLong(object + 0x34, x);
        SetLong(object + 0x38, z);
        const long ground = AH_CALL(AreaMap_Elevation)(Long(object + 0x34), z);
        SetWord(object + 0x3E, static_cast<unsigned>(ground));
    }
    Field_EdgeBits = static_cast<unsigned short>(Long(Mem(at::kLeaderZone)) - 5);
}

// ---- area 75 ----

// The press scenes' opening of a move (the tail's states 5 and 0xA): the
// phase 2, the move 0, the rhythm row 0; object A made Sprite_Current, its
// animation 2 ensured, its +0x2A = 1.
void OpenMoveA() {
    const unsigned index = B(at::kObjectA);
    B(at::kPhase) = 2;
    B(at::kMove) = 0;
    B(at::kRow) = 0;
    Sprite_Current = ObjectAt(index);
    AH_CALL(Sprite_EnsureAnimation)(2);
    Sprite_Current[0x2A] = 1;
}
// Rand() % 2 as the originals take it (and 0x80000001, the sign put back): -1,
// 0 or 1.
int RandHalf() { return AH_CALL(Rand)() % 2; }
// A counter word less (4 + the draw): 0xFFFC - r added as a word.
void DropCounter(std::uint32_t counter, int r) {
    AddWord(Mem(counter), static_cast<unsigned>(0xFFFCu - static_cast<unsigned>(r)));
}
// An effect record's dword +0xC less one.
void DrainEffect(unsigned slot) {
    unsigned char* const e = EffectAt(slot);
    SetLong(e + 0xC, Long(e + 0xC) - 1);
}
// |player counter - other counter|, the words as the originals subtract them.
int CounterGap() {
    const int d = static_cast<int>(Word(Mem(at::kPlayerCounter))) - static_cast<int>(Word(Mem(at::kOtherCounter)));
    return d < 0 ? -d : d;
}
// A byte of Input_Pressed's low byte (the originals test the byte 0x7E1BEC).
bool Pressed20() { return (B(at::kInputPressed) & 0x20) != 0; }

}  // namespace

// ===========================================================================
// Area 68 (descriptor 0x607D88): seven choices (three others' bodies), nine
// handlers (one another's), two glide state tables, object trigger 25.
// ===========================================================================

// original 0x40CEF0 (Area68_Choices[0]): the message word from
// Area68_ChoiceMessages by the s8 answer (unchecked; it stays in .data);
// answer 0: counter 0 = 0x14; 1: counter 0 = 3.
extern "C" void __cdecl Area68_ChoiceMessageA(void) {
    const auto answer = static_cast<signed char>(B(at::kChoiceAnswer));
    SetMessage(Word(Mem(static_cast<std::uint32_t>(static_cast<std::int32_t>(at::kArea68ChoiceMessages) + answer * 2))));
    if (answer == 0)
        B(at::kCounter0) = 0x14;
    else if (answer == 1)
        B(at::kCounter0) = 3;
}

// original 0x40CF20 (Area68_Choices[1]): the same from the table's second
// pair; answer 0: counter 0 = 0; 1: counter 0 = 0xFF.
extern "C" void __cdecl Area68_ChoiceMessageB(void) {
    const auto answer = static_cast<signed char>(B(at::kChoiceAnswer));
    SetMessage(Word(Mem(static_cast<std::uint32_t>(static_cast<std::int32_t>(at::kArea68ChoiceMessages + 4) + answer * 2))));
    if (answer == 0)
        B(at::kCounter0) = 0;
    else if (answer == 1)
        B(at::kCounter0) = 0xFF;
}

// original 0x40CF50 (Area68_Choices[2]): answer not 0: message 0x84 and the
// mark 6; 0: Inventory_CountUsed(1) at 0xF or more message 0x82, else 0x83 and
// the mark.
extern "C" void __cdecl Area68_ChoiceAskCount(void) {
    if (B(at::kChoiceAnswer) != 0) {
        SetMessage(0x84);
        B(at::kAnswerMark) = 6;
        return;
    }
    if (AH_CALL(Inventory_CountUsed)(1) >= 0xF) {
        SetMessage(0x82);
        return;
    }
    SetMessage(0x83);
    B(at::kAnswerMark) = 6;
}

// original 0x40CFA0 (Area68_Choices[5], [6]; Area50_Choices[3], [4]): answer
// not 0: message 0x84 and the mark 6; 0: message 0xFFFF.
extern "C" void __cdecl Area68_ChoiceAnswer84(void) {
    if (B(at::kChoiceAnswer) != 0) {
        SetMessage(0x84);
        B(at::kAnswerMark) = 6;
        return;
    }
    SetMessage(0xFFFF);
}

// original 0x40CFD0 (Area68_Handlers[0]; PSX 0x801F3A18): SpawnAtMember(1, 4,
// Area68_EffectKinds4).
extern "C" void __cdecl Area68_SpawnKind4AtMember1(void) { SpawnAtMember(1, 4, at::kArea68EffectKinds4); }
// original 0x40D020 (Area68_Handlers[1]; PSX 0x801F3A98): member 2.
extern "C" void __cdecl Area68_SpawnKind4AtMember2(void) { SpawnAtMember(2, 4, at::kArea68EffectKinds4); }

// original 0x40D070 (Area68_Handlers[2]; PSX 0x801F3B18): jmp through
// Area68_GlideStatesA by the running object's +4.
extern "C" void __cdecl Area68_GlideRunA(void) { RunState("Area68_GlideRunA", at::kArea68GlideStatesA); }

// original 0x40D090 (Area68_GlideStatesA[0] and Area68_GlideStatesB[0]):
// GlideBegin(0x10).
extern "C" void __cdecl Area68_GlideBegin10(void) { GlideBegin(0x10); }

// original 0x40D0C0 (Area68_GlideStatesA[1]): GlideStep over
// Area68_GlideStepsA.
extern "C" void __cdecl Area68_GlideStepA(void) { GlideStep(at::kArea68GlideStepsA); }

// original 0x40D130 (Area68_Handlers[3]; PSX 0x801F3C2C): jmp through
// Area68_GlideStatesB.
extern "C" void __cdecl Area68_GlideRunB(void) { RunState("Area68_GlideRunB", at::kArea68GlideStatesB); }

// original 0x40D150 (Area68_GlideStatesB[1]): GlideStep over
// Area68_GlideStepsB.
extern "C" void __cdecl Area68_GlideStepB(void) { GlideStep(at::kArea68GlideStepsB); }

// original 0x40D1C0 / 0x40D210 (Area68_Handlers[4], [5]; PSX 0x801F3D40,
// 0x801F3DC0): kind 1 from Area68_EffectKinds, members 1 and 2.
extern "C" void __cdecl Area68_SpawnKind1AtMember1(void) { SpawnAtMember(1, 1, at::kArea68EffectKinds); }
extern "C" void __cdecl Area68_SpawnKind1AtMember2(void) { SpawnAtMember(2, 1, at::kArea68EffectKinds); }
// original 0x40D260 / 0x40D2B0 (Area68_Handlers[7], [8]; PSX 0x801F3E48,
// 0x801F3EC8): kind 3.
extern "C" void __cdecl Area68_SpawnKind3AtMember1(void) { SpawnAtMember(1, 3, at::kArea68EffectKinds); }
extern "C" void __cdecl Area68_SpawnKind3AtMember2(void) { SpawnAtMember(2, 3, at::kArea68EffectKinds); }

// original 0x40D300 (Field_ObjectTriggers id 25; its arguments not read):
// ScriptFlags_Set40; the focus object's +1 = 4, +0x83 = 5, word +0x8A = 0
// (the pointer read again for each store); tail kind 4 with sub-kind 3;
// counter 3 = the focus object's index among the field objects. al 0.
extern "C" unsigned char __cdecl Area68_Trigger25(unsigned char*, unsigned char*) {
    AH_CALL(ScriptFlags_Set40)();
    area_harness::Pointer(at::kFocusObject)[1] = 4;
    area_harness::Pointer(at::kFocusObject)[0x83] = 5;
    SetWord(area_harness::Pointer(at::kFocusObject) + 0x8A, 0);
    const unsigned char index = ObjectIndex(area_harness::Pointer(at::kFocusObject));
    B(at::kTailKind) = 4;
    B(at::kTailSub) = 3;
    B(at::kCounter3) = index;
    return 0;
}

// ===========================================================================
// Area 69 (descriptor 0x608E98): one handler and its glide states.
// ===========================================================================

// original 0x40D360 (Area69_Handlers[0]; PSX 0x801F2C04): jmp through
// Area69_GlideStates by the running object's +4.
extern "C" void __cdecl Area69_GlideRun(void) { RunState("Area69_GlideRun", at::kArea69GlideStates); }

// original 0x40D380 (Area69_GlideStates[0]; also area 79's state table
// 0x60EC80 entry 0): GlideBegin(0x40).
extern "C" void __cdecl Area69_GlideBegin40(void) { GlideBegin(0x40); }

// original 0x40D3B0 (Area69_GlideStates[1]): GlideStep over Area69_GlideSteps.
extern "C" void __cdecl Area69_GlideStep(void) { GlideStep(at::kArea69GlideSteps); }

// ===========================================================================
// Area 71 (descriptor 0x609358): two handlers.
// ===========================================================================

// original 0x40D420 (Area71_Handlers[0]; PSX 0x801F2C04): Effect_FindFree into
// the scratch byte 0x903850; a slot: its +0 = 1, kind +5 = 0x3C, +0xB = the
// active member's index among the field objects (read after the call), and
// Sound_PlayEffect(0x207).
extern "C" void __cdecl Area71_SpawnEffect3C(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    B(at::kScratch) = slot;
    if (slot == 0xFF) return;
    const unsigned char member = ObjectIndex(Field_ActiveMember);
    unsigned char* const e = EffectAt(B(at::kScratch));
    e[0] = 1;
    e[5] = 0x3C;
    e[0xB] = member;
    AH_CALL(Sound_PlayEffect)(0x207);
}

// original 0x40D480 (Area71_Handlers[1]; PSX 0x801F2CF4): the same slot with
// +0 = 1 and kind 0x25, no member, no sound.
extern "C" void __cdecl Area71_SpawnEffect25(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    B(at::kScratch) = slot;
    if (slot == 0xFF) return;
    unsigned char* const e = EffectAt(B(at::kScratch));
    e[0] = 1;
    e[5] = 0x25;
}

// ===========================================================================
// Areas 72 and 73 (descriptors 0x609428, 0x6094F8): an init each.
// ===========================================================================

// original 0x40D4B0 (Area_Descriptors[72] +0x40; PSX 0x801F2D2C): a `jmp` over
// eleven nops to the body; PlaceRandomObject over Area72_Cells / _Weights.
extern "C" void __cdecl Area72_PlaceRandomObject(void) { PlaceRandomObject(at::kArea72Cells, at::kArea72Weights); }
// original 0x40D580 (Area_Descriptors[73] +0x40; PSX 0x801F2D2C, the same
// entry as area 72's): area 72's code over Area73_Cells / _Weights.
extern "C" void __cdecl Area73_PlaceRandomObject(void) { PlaceRandomObject(at::kArea73Cells, at::kArea73Weights); }

// ===========================================================================
// Area 74 (descriptor 0x60A2C0): five choices (two others'), seven handlers
// (the choices' bodies among them), object trigger 28.
// ===========================================================================

// original 0x40D650 (Area74_Choices[0] = Area74_Handlers[2]): answer not 0:
// message 0x94 and the mark 6; 0: story flag 0x365 (the byte 0x90409C bit
// 0x20) set message 0x92, else 0x93 and the mark.
extern "C" void __cdecl Area74_ChoiceAsk92(void) {
    if (B(at::kChoiceAnswer) != 0) {
        SetMessage(0x94);
        B(at::kAnswerMark) = 6;
        return;
    }
    if (B(at::kFlagByte9C) & 0x20) {
        SetMessage(0x92);
        return;
    }
    SetMessage(0x93);
    B(at::kAnswerMark) = 6;
}

// original 0x40D690 (Area74_Choices[3], [4] = Area74_Handlers[5], [6]):
// answer not 0: message 0x94 and the mark 6; 0: message 0xFFFF.
extern "C" void __cdecl Area74_ChoiceAnswer94(void) {
    if (B(at::kChoiceAnswer) != 0) {
        SetMessage(0x94);
        B(at::kAnswerMark) = 6;
        return;
    }
    SetMessage(0xFFFF);
}

// original 0x40D6C0 (Area74_Handlers[0]; PSX 0x801F2D48): the active member's
// +0x80 bit 0 cleared; with the leader's +0x89 at 2, the running object's +9 at
// 0 and the leader's direction (+8 & 7) 5 or 1: the running object turned to
// it; unless Field_ObjectBlockedAhead(the active member): its +0x87 = 3,
// MoveCmd_Move(the script object, the direction); direction 5 toggles Cond
// row 7's flag 6 off (Flags_Clear) or sets flag 5; direction 1 sets flag 6;
// then Sound_PlayEffect(the leader's dword +0x2C + 0x100).
extern "C" void __cdecl Area74_PushToggleRow7(void) {
    unsigned char* const member = Field_ActiveMember;
    member[0x80] = static_cast<unsigned char>(member[0x80] & 0xFE);
    if (B(at::kLeaderByte89) != 2) return;
    unsigned char* const cur = Sprite_Current;
    const auto dir = static_cast<unsigned char>(B(at::kLeaderDir) & 7);
    if (cur[9] != 0) return;
    if (dir != 5 && dir != 1) return;
    cur[8] = dir;
    if (AH_CALL(Field_ObjectBlockedAhead)(Field_ActiveMember) != 0) return;
    Field_ActiveMember[0x87] = 3;
    AH_CALL(MoveCmd_Move)(MoveScript_Object, dir);
    if (dir == 5) {
        if (AH_CALL(Flags_Test)(Mem(at::kCondRow7), 6) != 0)
            AH_CALL(Flags_Clear)(Mem(at::kCondRow7), 6);
        else
            AH_CALL(Flags_Set)(Mem(at::kCondRow7), 5);
    } else {
        AH_CALL(Flags_Set)(Mem(at::kCondRow7), 6);
    }
    AH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(Long(Mem(at::kLeaderSound)) + 0x100));
}

// original 0x40D790 (Area74_Handlers[1]; PSX 0x801F2E74; also named three
// times by Area_ObjectHandler's fallback table 0x660CA0): the active member's
// +0x80 bit 0 cleared.
extern "C" void __cdecl Area74_ClearMemberBit0(void) {
    Field_ActiveMember[0x80] = static_cast<unsigned char>(Field_ActiveMember[0x80] & 0xFE);
}

// original 0x40D7A0 (Field_ObjectTriggers id 28; its arguments not read):
// ScriptFlags_Set40, tail kind 4 with sub-kind 7. al 0.
extern "C" unsigned char __cdecl Area74_Trigger28(unsigned char*, unsigned char*) {
    AH_CALL(ScriptFlags_Set40)();
    B(at::kTailKind) = 4;
    B(at::kTailSub) = 7;
    return 0;
}

// ===========================================================================
// Area 75 (descriptor 0x60AA98): fifteen choices (choices 2..14 are handlers
// 0..12; handlers 8 and 9 are other blocks'), three handler state tables,
// the step hook, object trigger 41, tail kind 30 and its press phases.
// ===========================================================================

// original 0x40D7C0 (Area75_Choices[2] = Area75_Handlers[0]): Party_DropIn(0).
extern "C" void __cdecl Area75_DropIn0(void) { AH_CALL(Party_DropIn)(0); }

// original 0x40D7D0 (choice 3 = handler 1): ScriptFlags_Set40, tail kind 30
// at state 0.
extern "C" void __cdecl Area75_ArmTail30(void) {
    AH_CALL(ScriptFlags_Set40)();
    B(at::kTailKind) = 0x1E;
    B(at::kTailState) = 0;
}

// original 0x40D7F0 (choice 4 = handler 2): object B's index = the active
// member's among the field objects.
extern "C" void __cdecl Area75_MarkObjectB(void) { B(at::kObjectB) = ObjectIndex(Field_ActiveMember); }

// original 0x40D820 (choice 5 = handler 3): object A's index, the same way.
extern "C" void __cdecl Area75_MarkObjectA(void) { B(at::kObjectA) = ObjectIndex(Field_ActiveMember); }

// original 0x40D850 (choice 6 = handler 4): the two effect records' +1 = 2
// (the other's first).
extern "C" void __cdecl Area75_ShowEffects(void) {
    const unsigned other = B(at::kOtherEffect);
    const unsigned player = B(at::kPlayerEffect);
    EffectAt(other)[1] = 2;
    EffectAt(player)[1] = 2;
}

// original 0x40D880 (choice 7 = handler 5): field object 5's +0x83 = 7, word
// +0x8A = 0, +0x84 = 2, +1 = 4.
extern "C" void __cdecl Area75_PoseObject5(void) {
    unsigned char* const object = ObjectAt(5);
    object[0x83] = 7;
    SetWord(object + 0x8A, 0);
    object[0x84] = 2;
    object[1] = 4;
}

// original 0x40D8A0 (choice 8 = handler 6): five times, Sprite_FindFree into
// the scratch word 0x903850; a slot: EventOp_0x(Area75_SpawnOps record i), and
// the new Sprite_Current put at the running object's x + 0x30000, its z and its
// dword +0x3C (each read after the op), its +0xB = i. Then Sprite_Current and
// Field_ActiveMember put back as they were on entry.
extern "C" void __cdecl Area75_SpawnFive(void) {
    unsigned char* const member = Field_ActiveMember;
    unsigned char* const self = Sprite_Current;
    std::uint32_t op = at::kArea75SpawnOps;
    for (unsigned char i = 0; i < 5; ++i, op += at::kArea75SpawnOpStride) {
        const unsigned char slot = AH_CALL(Sprite_FindFree)();
        SetWord(Mem(at::kScratch), slot);
        if (slot == 0xFF) continue;
        AH_CALL(EventOp_0x)(Mem(op));
        const std::int32_t x = Long(self + 0x34);
        SetLong(Sprite_Current + 0x34, static_cast<std::int32_t>(static_cast<std::uint32_t>(x) + 0x30000u));
        const std::int32_t z = Long(self + 0x38);
        SetLong(Sprite_Current + 0x38, z);
        const std::int32_t y = Long(self + 0x3C);
        SetLong(Sprite_Current + 0x3C, y);
        Sprite_Current[0xB] = i;
    }
    Sprite_Current = self;
    Field_ActiveMember = member;
}

// original 0x40D920 (choice 9 = handler 7): jmp through Area75_FlyStates by
// the running object's +4.
extern "C" void __cdecl Area75_FlyRun(void) { RunState("Area75_FlyRun", at::kArea75FlyStates); }

// original 0x40D940 (Area75_FlyStates[0]): the running object's dword +0x14 =
// Area75_FlyHeights[its +0xB] (unchecked; it stays in .data), +0x2B = 3, state
// 1; the script object's position word +0xA back by 2.
extern "C" void __cdecl Area75_FlyBegin(void) {
    unsigned char* const cur = Sprite_Current;
    SetLong(cur + 0x14, B(at::kArea75FlyHeights + cur[0xB]));
    Sprite_Current[0x2B] = 3;
    Sprite_Current[4] = 1;
    AddWord(MoveScript_Object + 0xA, 0xFFFE);
}

// original 0x40D980 (Area75_FlyStates[1]): the running object's +0x5D at 0x80:
// its +0 = 0 (gone). Else +0x5D less 8, +0x5E and +0x5F plus 0xF8 (bytes);
// x +0x34 plus +0xC, z +0x38 plus +0x10, word +0x3E plus word +0x14; the
// script position back by 2.
extern "C" void __cdecl Area75_FlyStep(void) {
    unsigned char* const cur = Sprite_Current;
    if (cur[0x5D] == 0x80) {
        cur[0] = 0;
        return;
    }
    cur[0x5D] = static_cast<unsigned char>(cur[0x5D] - 8);
    cur[0x5E] = static_cast<unsigned char>(cur[0x5E] + 0xF8);
    cur[0x5F] = static_cast<unsigned char>(cur[0x5F] + 0xF8);
    SetLong(cur + 0x34, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(cur + 0x34)) + static_cast<std::uint32_t>(Long(cur + 0xC))));
    SetLong(cur + 0x38, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(cur + 0x38)) + static_cast<std::uint32_t>(Long(cur + 0x10))));
    AddWord(cur + 0x3E, Word(cur + 0x14));
    AddWord(MoveScript_Object + 0xA, 0xFFFE);
}

// original 0x40D9F0 (choice 12 = handler 10): one frame in four the two effect
// records' dwords +0xC less 1 (the other's first); counter 0 below 0x20: the
// script position back by 2.
extern "C" void __cdecl Area75_DrainEffects(void) {
    const unsigned bit = (Frame_Counter & 3) == 0 ? 1u : 0u;
    unsigned char* e = EffectAt(B(at::kOtherEffect));
    SetLong(e + 0xC, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(e + 0xC)) - bit));
    e = EffectAt(B(at::kPlayerEffect));
    SetLong(e + 0xC, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(e + 0xC)) - bit));
    if (B(at::kCounter0) < 0x20) AddWord(MoveScript_Object + 0xA, 0xFFFE);
}

// original 0x40DA60 (choice 13 = handler 11): jmp through Area75_FollowStates
// by the running object's +4.
extern "C" void __cdecl Area75_FollowRun(void) { RunState("Area75_FollowRun", at::kArea75FollowStates); }

// original 0x40DA80 (Area75_FollowStates[0]): Sound_PlayById(0x200), the
// script position back by 2, the running object's state 1 (both read after
// the call).
extern "C" void __cdecl Area75_FollowBegin(void) {
    AH_CALL(Sound_PlayById)(0x200);
    AddWord(MoveScript_Object + 0xA, 0xFFFE);
    Sprite_Current[4] = 1;
}

// original 0x40DAB0 (Area75_FollowStates[1]): Field_Request not 0: the running
// object's z = Field_Kind2Z and the script position back by 2; 0: its state 0.
extern "C" void __cdecl Area75_FollowStep(void) {
    if (Field_Request != 0) {
        SetLong(Sprite_Current + 0x38, Field_Kind2Z);
        AddWord(MoveScript_Object + 0xA, 0xFFFE);
        return;
    }
    Sprite_Current[4] = 0;
}

// original 0x40DAE0 (choice 14 = handler 12): Field_Request 5:
// Sound_PlayEffect(0x207); else the running object's z = Field_Kind2Z and the
// script position back by 2.
extern "C" void __cdecl Area75_FollowKind2Z(void) {
    if (Field_Request == 5) {
        AH_CALL(Sound_PlayEffect)(0x207);
        return;
    }
    SetLong(Sprite_Current + 0x38, Field_Kind2Z);
    AddWord(MoveScript_Object + 0xA, 0xFFFE);
}

// original 0x40DB20 (Area75_Choices[0]): answer 0: message 0x19, the tail
// state 0x14, counter 0 = 0x15; else message 0x1A.
extern "C" void __cdecl Area75_ChoiceStart14(void) {
    if (B(at::kChoiceAnswer) != 0) {
        SetMessage(0x1A);
        return;
    }
    SetMessage(0x19);
    B(at::kTailState) = 0x14;
    B(at::kCounter0) = 0x15;
}

// original 0x40DB50 (Area75_Choices[1]): answer 0: message 0xFFFF, the tail
// state 0xF, its timer 0x1E, counter 0 = 0x14; else message 0x18.
extern "C" void __cdecl Area75_ChoiceStartF(void) {
    if (B(at::kChoiceAnswer) != 0) {
        SetMessage(0x18);
        return;
    }
    SetMessage(0xFFFF);
    B(at::kTailState) = 0xF;
    SetWord(Mem(at::kTailTimer), 0x1E);
    B(at::kCounter0) = 0x14;
}

// original 0x40DB90 (Field_ModeTailKinds[30], armed by Area75_ArmTail30): by
// the s8 state 0x9039F4 through a 36-byte table and a 24-entry jump table in
// the body (0..0x23; any other: out). Each state leaves the state it ends on
// (the register the original carries to the exit: the new state, or the one it
// entered with when it waits), and every exit with that state in 6..0xE ends in
// Area75_PhaseRun (a tail jmp). States 0..4 and 0x1F..0x23 are the scene's
// fades, area changes and music; 5..0xD three rounds (the other's turn, the
// player's, the two at once) by counter 0; 0xF a timer back to 4; 0x14 / 0x15
// a round of both from a choice; 0x1E the counts' words bumped.
extern "C" void __cdecl Area75_TailPresses(void) {
    auto cl = B(at::kTailState);
    switch (static_cast<signed char>(cl)) {
    case 0:
        AH_CALL(Transition_Start)(0);
        cl = 1;
        B(at::kTailState) = cl;
        break;
    case 1:
        if (MoveScript_WaitWordDA != 0) break;
        Draw_PassFlags = 0;
        cl = 2;
        B(at::kTailState) = cl;
        break;
    case 2: {
        AH_CALL(Flags_Set)(Mem(at::kStoryFlags), 0x41);
        AH_CALL(Field_ChangeArea)(0x4B, 0x170000, 0x190000, 0x82);
        B(at::kMusicCurrent) = 0x58;
        const unsigned char set = AH_CALL(Flags_Test)(Mem(at::kCondRow10), 1);
        cl = 3;
        B(at::kTailState) = cl;
        B(at::kCounter0) = set != 0 ? 0x13 : 0;
        break;
    }
    case 3:
        if (MoveScript_WaitWordDA != 0) break;
        Draw_PassFlags = 0x1F;
        AH_CALL(Transition_Start)(1);
        cl = 4;
        B(at::kTailState) = cl;
        break;
    case 4:
        if (MoveScript_WaitWordDA != 0) break;
        cl = 5;
        B(at::kCounter0) = 1;
        B(at::kTailState) = cl;
        break;
    case 5: {
        if (B(at::kCounter0) != 3) break;
        AH_CALL(Area75_ResetPresses)();
        const unsigned index = B(at::kObjectA);
        B(at::kPressFlags) = static_cast<unsigned char>(B(at::kPressFlags) | 8);
        B(at::kPhase) = 2;
        B(at::kMove) = 0;
        B(at::kRow) = 0;
        Sprite_Current = ObjectAt(index);
        AH_CALL(Sprite_EnsureAnimation)(2);
        Sprite_Current[0x2A] = 1;
        cl = 6;
        B(at::kTailState) = cl;
        break;
    }
    case 6:
        if (Word(Mem(at::kOtherCounter)) > 0x578) break;
        cl = 7;
        B(at::kPhase) = 0;
        B(at::kCounter0) = static_cast<unsigned char>(B(at::kCounter0) + 1);
        B(at::kTailState) = cl;
        break;
    case 7:
        if (B(at::kCounter0) != 6) break;
        cl = 8;
        B(at::kPhase) = 2;
        B(at::kMove) = 1;
        B(at::kTailState) = cl;
        break;
    case 8:
        if (Word(Mem(at::kPlayerCounter)) > 0x578) break;
        cl = 9;
        B(at::kPhase) = 0;
        B(at::kCounter0) = static_cast<unsigned char>(B(at::kCounter0) + 1);
        B(at::kTailState) = cl;
        break;
    case 9: {
        if (B(at::kCounter0) != 9) break;
        AH_CALL(Msg_OpenScript)(0x17);
        const auto flags = static_cast<unsigned char>(B(at::kPressFlags) | 4);
        cl = 0xA;
        Field_Request = 2;
        B(at::kPressFlags) = flags;
        B(at::kTailState) = cl;
        break;
    }
    case 0xA:
        if (Field_Request == 2) break;
        OpenMoveA();
        cl = 0xB;
        B(at::kTailState) = cl;
        break;
    case 0xB:
        if (CounterGap() <= 0x96) break;
        cl = 0xC;
        B(at::kTailState) = cl;
        B(at::kCounter0) = static_cast<unsigned char>(B(at::kCounter0) + 1);
        break;
    case 0xC:
        if (Word(Mem(at::kOtherCounter)) > 0x47E) break;
        cl = 0xD;
        B(at::kPhase) = 0;
        B(at::kCounter0) = static_cast<unsigned char>(B(at::kCounter0) + 1);
        B(at::kTailState) = cl;
        break;
    case 0xD:
        if (B(at::kCounter0) != 0xC) break;
        AH_CALL(Area75_ResetPresses)();
        cl = 0xE;
        B(at::kTailState) = cl;
        break;
    case 0xF: {
        const auto timer = static_cast<std::uint16_t>(Word(Mem(at::kTailTimer)) - 1);
        SetWord(Mem(at::kTailTimer), timer);
        if (timer != 0) break;
        cl = 4;
        B(at::kTailState) = cl;
        break;
    }
    case 0x14: {
        if (Field_Request == 2) break;
        AH_CALL(Area75_ResetPresses)();
        const unsigned a = B(at::kObjectA);
        B(at::kPhase) = 2;
        B(at::kMove) = 0;
        SetWord(Mem(at::kMoveFrames), 0x78);
        Sprite_Current = ObjectAt(a);
        AH_CALL(Sprite_EnsureAnimation)(2);
        Sprite_Current[0x2A] = 1;
        const unsigned b = B(at::kObjectB);
        Sprite_Current = ObjectAt(b);
        AH_CALL(Sprite_EnsureAnimation)(2);
        Sprite_Current[0x2A] = 0;
        AH_CALL(Sound_PlayEffect)(0x201);
        cl = 0x15;
        B(at::kTailState) = cl;
        break;
    }
    case 0x15:
        AH_CALL(Area75_PhaseRun)();
        cl = B(at::kTailState);
        break;
    case 0x1E: {
        const unsigned a = B(at::kObjectA);
        cl = 0x1F;
        B(at::kTailState) = cl;
        AddWord(ObjectAt(a) + 0x8A, 1);
        AddWord(Mem(at::kLeaderWord12E), 1);
        break;
    }
    case 0x1F:
        if (B(at::kCounter0) != 0x1A) break;
        AH_CALL(Transition_Start)(0);
        AH_CALL(Music_FadeOut)(0x20);
        cl = 0x20;
        B(at::kTailState) = cl;
        break;
    case 0x20:
        if (MoveScript_WaitWordDA != 0) break;
        Draw_PassFlags = 0;
        AH_CALL(Music_FadeOutStop)(0xA);
        AH_CALL(Sound_LoadStream)(6);
        AH_CALL(Msg_OpenScript)(0x1B);
        cl = 0x21;
        Field_Request = 2;
        B(at::kTailState) = cl;
        break;
    case 0x21:
        if (Field_Request == 2) break;
        if (AH_CALL(Sound_StreamDone)() == 0) {
            cl = B(at::kTailState);
            break;
        }
        cl = 0x22;
        B(at::kTailState) = cl;
        break;
    case 0x22:
        AH_CALL(Flags_Clear)(Mem(at::kStoryFlags), 0x41);
        AH_CALL(Field_ChangeArea)(0x4B, 0x198000, 0x1B8000, 0x83);
        cl = 0x23;
        B(at::kTailState) = cl;
        break;
    case 0x23:
        if (MoveScript_WaitWordDA != 0) break;
        Draw_PassFlags = 0x1F;
        AH_CALL(Transition_Start)(1);
        B(at::kCounter0) = 1;
        AH_CALL(ScriptFlags_Clear40)();
        cl = 0;
        B(at::kTailKind) = 0;
        B(at::kTailState) = cl;
        break;
    default:   // 0xE, 0x10..0x13, 0x16..0x1D: the table's `out`; above 0x23 or negative: `ja` to it
        break;
    }
    if (static_cast<unsigned char>(cl - 6) < 9) AH_CALL(Area75_PhaseRun)();
}

// original 0x40E120 (Area_StepHook's case for area 75): unless Cond row 10's
// flag 2, with x at 0x1B0000 or less (signed) and z's high word in
// 0x38..0x3B: ScriptFlags_Set40, counter 3 = 0xA, al 1. Else al 0.
extern "C" int __cdecl Area75_StepHook(long x, long z) {
    if (AH_CALL(Flags_Test)(Mem(at::kCondRow10), 2) != 0) return 0;
    if (x > 0x1B0000) return 0;
    if (static_cast<std::uint16_t>((static_cast<std::uint32_t>(z) >> 16) - 0x38) >= 4) return 0;
    AH_CALL(ScriptFlags_Set40)();
    B(at::kCounter3) = 0xA;
    return 1;
}

// original 0x40E160 (Field_ObjectTriggers id 41; its arguments not read):
// ScriptFlags_Set40, tail kind 0x2C at state 0 with sub-kind 0xC. al 0.
extern "C" unsigned char __cdecl Area75_Trigger41(unsigned char*, unsigned char*) {
    AH_CALL(ScriptFlags_Set40)();
    B(at::kTailKind) = 0x2C;
    B(at::kTailState) = 0;
    B(at::kTailSub) = 0xC;
    return 0;
}

// original 0x40E180 (called by the tail's states 5, 0xD and 0x14): the phase,
// the flags, the two press counts, the list and its position 0; both counters
// 0x5DC (the other's first).
extern "C" void __cdecl Area75_ResetPresses(void) {
    B(at::kPhase) = 0;
    B(at::kPressFlags) = 0;
    B(at::kEarlyPresses) = 0;
    B(at::kIdleFrames) = 0;
    B(at::kList) = 0;
    B(at::kListPos) = 0;
    SetWord(Mem(at::kOtherCounter), 0x5DC);
    SetWord(Mem(at::kPlayerCounter), 0x5DC);
}

// original 0x40E1C0 (called by the tail's state 0x15, tail-jumped to by its
// exit): call Area75_Phases[the phase byte], then jmp Area75_DrawCounters. The
// original indexes the five-entry table unchecked (a phase of 5 or more calls
// through the bytes after it): ours aborts there.
extern "C" void __cdecl Area75_PhaseRun(void) {
    const unsigned phase = B(at::kPhase);
    if (phase >= at::kArea75PhaseCount)
        bof3::Fatal("Area75_PhaseRun: the phase byte %u is past Area75_Phases' %u entries (the original calls through "
                    "the bytes after it)",
                    phase, at::kArea75PhaseCount);
    reinterpret_cast<area_harness::Handler>(
        static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(at::kArea75Phases + phase * 4)))))();
    AH_CALL(Area75_DrawCounters)();
}

// original 0x40E1E0 (Area75_Phases[1]): the move read first. The player's
// counter at 0x1F4 or less: Sound_PlayEffect(0x201), object B's animation
// Area75_MoveAnims[move * 4 + 2] if not 0 (Sprite_Current made object B, its
// +0x2A 0), phase 4. Else a deal: both press counts 0; the rhythm row from
// Area75_Rows[Rand() & 0xF]; the list position on by one, and past the list's
// count a new list from Area75_NextList[Rand() & 0xF] at position 0; the
// move = the list's pair's first byte less 1, its frames the second; object
// B's animation Area75_MoveAnims[old move * 4 + new move] if not 0;
// Sound_PlayEffect(0x201); phase 2. The list byte indexes Area75_Lists
// unchecked (the byte compare stays in .data; the pointer read past three
// faults): ours aborts there.
extern "C" void __cdecl Area75_PhaseDeal(void) {
    const unsigned prev = B(at::kMove);
    if (Word(Mem(at::kPlayerCounter)) <= 0x1F4) {
        AH_CALL(Sound_PlayEffect)(0x201);
        const unsigned char anim = B(at::kArea75MoveAnims + 2 + prev * 4);
        if (anim != 0) {
            Sprite_Current = ObjectAt(B(at::kObjectB));
            AH_CALL(Sprite_EnsureAnimation)(anim);
            Sprite_Current[0x2A] = 0;
        }
        B(at::kPhase) = 4;
        return;
    }
    B(at::kEarlyPresses) = 0;
    B(at::kIdleFrames) = 0;
    B(at::kRow) = B(at::kArea75Rows + (AH_CALL(Rand)() & 0xF));
    unsigned list = B(at::kList);
    auto pos = static_cast<unsigned char>(B(at::kListPos) + 1);
    B(at::kListPos) = pos;
    if (pos >= B(at::kArea75Lists + 4 + list * 8)) {
        list = B(at::kArea75NextList + (AH_CALL(Rand)() & 0xF));
        pos = 0;
        B(at::kList) = static_cast<unsigned char>(list);
        B(at::kListPos) = 0;
    }
    if (list >= at::kArea75ListCount)
        bof3::Fatal("Area75_PhaseDeal: the list byte %u is past Area75_Lists' %u records (the original follows the "
                    "bytes after them as a pointer)",
                    list, at::kArea75ListCount);
    const unsigned char* const pair =
        Mem(static_cast<std::uint32_t>(Long(Mem(at::kArea75Lists + list * 8)))) + pos * 2;
    const auto move = static_cast<unsigned char>(pair[0] - 1);
    B(at::kMove) = move;
    SetWord(Mem(at::kMoveFrames), pair[1]);
    const unsigned char anim = B(at::kArea75MoveAnims + move + prev * 4);
    if (anim != 0) {
        Sprite_Current = ObjectAt(B(at::kObjectB));
        AH_CALL(Sprite_EnsureAnimation)(anim);
        Sprite_Current[0x2A] = 0;
    }
    AH_CALL(Sound_PlayEffect)(0x201);
    B(at::kPhase) = 2;
}

// original 0x40E330 (Area75_Phases[2]): by the move - 0: Area75_OtherPress and
// Area75_EarlyPress; 1: Area75_PlayerPress; 2: Area75_OtherPress and
// Area75_PlayerPress; any other: Area75_EarlyPress. Then the move's frames
// less 1; at 0, phase 1 unless the flags' bit 8 (then the frames 0x2710).
// Without bit 8: more than 3 presses out of turn, more than 0x1E idle frames
// or the counters more than 0xC8 apart: phase 3.
extern "C" void __cdecl Area75_PhasePlay(void) {
    switch (B(at::kMove)) {
    case 0:
        AH_CALL(Area75_OtherPress)();
        AH_CALL(Area75_EarlyPress)();
        break;
    case 1: AH_CALL(Area75_PlayerPress)(); break;
    case 2:
        AH_CALL(Area75_OtherPress)();
        AH_CALL(Area75_PlayerPress)();
        break;
    default: AH_CALL(Area75_EarlyPress)(); break;
    }
    const auto frames = static_cast<std::uint16_t>(Word(Mem(at::kMoveFrames)) - 1);
    SetWord(Mem(at::kMoveFrames), frames);
    const unsigned char flags = B(at::kPressFlags);
    if (frames == 0) {
        if ((flags & 8) == 0)
            B(at::kPhase) = 1;
        else
            SetWord(Mem(at::kMoveFrames), 0x2710);
    }
    if (flags & 8) return;
    if (B(at::kEarlyPresses) > 3 || B(at::kIdleFrames) > 0x1E || CounterGap() > 0xC8) B(at::kPhase) = 3;
}

// original 0x40E3C0 (Area75_Phases[3]): Cond row 10's flag 1 set, the tail
// state 0x1E.
extern "C" void __cdecl Area75_PhaseMissed(void) {
    AH_CALL(Flags_Set)(Mem(at::kCondRow10), 1);
    B(at::kTailState) = 0x1E;
}

// original 0x40E3E0 (Area75_Phases[4]): story flag 0x41 cleared; with
// Cond_ByteFA at 0xA: Cond row 10's flag 0 set; the tail kind and state 0,
// the run step 0; object B's +0x83 = 4 and word +0x8A = 0; MoveScript_Var7 5;
// object A's +0x83 = 5 and word +0x8A = 0; Party_DropIn(5); counter 0 = 0x1E.
extern "C" void __cdecl Area75_PhaseReached(void) {
    AH_CALL(Flags_Clear)(Mem(at::kStoryFlags), 0x41);
    if (Cond_ByteFA != 0xA) return;
    AH_CALL(Flags_Set)(Mem(at::kCondRow10), 0);
    const unsigned b = B(at::kObjectB);
    const unsigned a = B(at::kObjectA);
    B(at::kTailKind) = 0;
    B(at::kTailState) = 0;
    B(at::kVar7Step) = 0;
    unsigned char* const object_b = ObjectAt(b);
    object_b[0x83] = 4;
    SetWord(object_b + 0x8A, 0);
    unsigned char* const object_a = ObjectAt(a);
    MoveScript_Var7 = 5;
    object_a[0x83] = 5;
    SetWord(object_a + 0x8A, 0);
    AH_CALL(Party_DropIn)(5);
    B(at::kCounter0) = 0x1E;
}

// original 0x40E480 (called by Area75_PhasePlay for moves 0 and 2): the
// other's press when Area75_Rhythm[row * 15 + frames % 15] is not 0 (the
// table read unchecked; it stays in .data): Sound_PlayEffect(0x202); the
// other's counter less 4 + Rand() % 2; its effect's dword +0xC less 1; object
// A made Sprite_Current, its +0x4A = 1; jmp Sprite_ScriptTick (its answer the
// caller's to ignore).
extern "C" void __cdecl Area75_OtherPress(void) {
    const unsigned rem = Word(Mem(at::kMoveFrames)) % 15;
    if (B(at::kArea75Rhythm + rem + B(at::kRow) * 15u) == 0) return;
    AH_CALL(Sound_PlayEffect)(0x202);
    const int r = RandHalf();
    DropCounter(at::kOtherCounter, r);
    DrainEffect(B(at::kOtherEffect));
    Sprite_Current = ObjectAt(B(at::kObjectA));
    Sprite_Current[0x4A] = 1;
    AH_CALL(Sprite_ScriptTick)();
}

// original 0x40E520 (called by Area75_PhasePlay for moves 1 and 2): the
// button (Input_Pressed bit 0x20): Sound_PlayEffect(0x202); the player's
// counter less 4 + Rand() % 2; Sprite_Current the leader's record, its +0x4A
// = 1; the player's effect's +0xC less 1; Sprite_ScriptTick; the idle frames
// 0. No button: the idle frames + 1.
extern "C" void __cdecl Area75_PlayerPress(void) {
    if (!Pressed20()) {
        B(at::kIdleFrames) = static_cast<unsigned char>(B(at::kIdleFrames) + 1);
        return;
    }
    AH_CALL(Sound_PlayEffect)(0x202);
    const int r = RandHalf();
    const unsigned slot = B(at::kPlayerEffect);
    DropCounter(at::kPlayerCounter, r);
    Sprite_Current = Mem(at::kLeader);
    B(at::kLeader4A) = 1;
    DrainEffect(slot);
    AH_CALL(Sprite_ScriptTick)();
    B(at::kIdleFrames) = 0;
}

// original 0x40E5A0 (called by Area75_PhasePlay for move 0 and moves past 2):
// unless the flags' bit 8, the button: Sprite_Current the leader's record, its
// +0x4A = 1, the player's effect's +0xC less 1, Sprite_ScriptTick, the presses
// out of turn + 1.
extern "C" void __cdecl Area75_EarlyPress(void) {
    if (B(at::kPressFlags) & 8) return;
    if (!Pressed20()) return;
    Sprite_Current = Mem(at::kLeader);
    const unsigned slot = B(at::kPlayerEffect);
    B(at::kLeader4A) = 1;
    DrainEffect(slot);
    AH_CALL(Sprite_ScriptTick)();
    B(at::kEarlyPresses) = static_cast<unsigned char>(B(at::kEarlyPresses) + 1);
}

// original 0x40E5F0 (jmp'd to by Area75_PhaseRun): unless the flags' bit 1,
// each counter in a window (Area75_DrawWindow at (0x18, 0x20) and (0x90,
// 0x20), 0x54 x 0x11) and printed as (word / 100, word % 100), each a byte,
// by Crt_sprintf(0x904BA0, Area75_CounterFormat, ...) and Text_DrawAt at
// (0x1E, 0x22) / (0x96, 0x22), 6 characters, colour 2 when the counters are
// more than 0x96 apart, else 0. With the flags' bit 4 the text is drawn only
// on frames whose counter has a bit of 0xC. The original pushes the colour as
// a dword whose upper bytes are its own stack's (Text_DrawString reads the
// low byte only).
extern "C" void __cdecl Area75_DrawCounters(void) {
    const bool show = (B(at::kPressFlags) & 4) ? (Frame_Counter & 0xC) != 0 : true;
    const unsigned first = Word(Mem(at::kPlayerCounter));
    const unsigned color = CounterGap() <= 0x96 ? 0u : 2u;
    if (B(at::kPressFlags) & 1) return;
    unsigned char* const text = Mem(at::kTextBuffer);
    const char* const format = reinterpret_cast<const char*>(Mem(at::kArea75Format));
    AH_CALL(Area75_DrawWindow)(0x18, 0x20, 0x54, 0x11, 0);
    AH_CALL(Crt_sprintf)(reinterpret_cast<char*>(text), format, static_cast<unsigned char>(first / 100),
                         static_cast<unsigned char>(first % 100));
    if (show) AH_CALL(Text_DrawAt)(0x1E, 0x22, static_cast<int>(color), 6, text);
    const unsigned second = Word(Mem(at::kOtherCounter));
    AH_CALL(Area75_DrawWindow)(0x90, 0x20, 0x54, 0x11, 0);
    AH_CALL(Crt_sprintf)(reinterpret_cast<char*>(text), format, static_cast<unsigned char>(second / 100),
                         static_cast<unsigned char>(second % 100));
    if (show) AH_CALL(Text_DrawAt)(0x96, 0x22, static_cast<int>(color), 6, text);
}

// original 0x40E750 (called by Area75_DrawCounters and by area 42's timer tail
// 0x406A30): a window of (x, y, w, h), `flag` choosing the shade (0xFF, else
// 0xAC), the texture page (0x2F, else 0xF) and the CLUT row: a draw-mode
// primitive (its RECT (0, 0xF0, 0x10, 0x10) in the 8 bytes before it), four
// semi-transparent FT4 quads with float corners - the left edge 2 wide, the
// middle in two halves of (w + 1 - 4) >> 1 (the right one a pixel wider for an
// odd w + 1), the right edge 2 wide, each the height h + 1 with its corners
// cut by 2 / 3 / 1 as the texture's - each with Gpu_GetClut(flag * 32 + 0x10,
// 0x1E1); a second draw-mode primitive (RECT (0, 0, 0x100, 0x100)); then
// Menu_DrawOutline(x + 2, y + 2, w - 4, h - 4, flag). x, y and w + 1, h + 1
// are taken as words for the corners, as dwords for the outline.
extern "C" void __cdecl Area75_DrawWindow(int x, int y, int w, int h, int flag) {
    const auto flag8 = static_cast<unsigned char>(flag);
    const unsigned char shade = flag8 != 0 ? 0xFF : 0xAC;
    const unsigned tpage = (flag8 != 0 ? 0x20u : 0u) | 0xFu;
    const auto clut_x = static_cast<int>((static_cast<unsigned>(flag) & 0xFF) << 5) + 0x10;
    // the first draw mode
    unsigned char* const rect0 = Gfx_PacketNext;
    Gfx_PacketNext = rect0 + 8;
    SetWord(rect0, 0);
    SetWord(rect0 + 2, 0xF0);
    SetWord(rect0 + 4, 0x10);
    SetWord(rect0 + 6, 0x10);
    AH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage,
                             static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(rect0)));
    AH_CALL(Gfx_CommitPrim)(1, 0xC);
    const auto w1 = static_cast<std::uint32_t>(w) + 1;
    const auto h1 = static_cast<std::uint32_t>(h) + 1;
    const int xs = static_cast<int>(static_cast<std::uint32_t>(x) & 0xFFFF);
    const int ys = static_cast<int>(static_cast<std::uint32_t>(y) & 0xFFFF);
    const int h1w = static_cast<int>(h1 & 0xFFFF);
    const auto hb = static_cast<unsigned char>(h1);
    const float fx = static_cast<float>(xs);
    const float fy = static_cast<float>(ys);
    const float fx2 = static_cast<float>(xs + 2);
    const float fyh = static_cast<float>(ys + h1w);
    const float fy2 = fy + 2.0f;     // + the constant 0x5C41C0
    const float fyh3 = fyh - 3.0f;   // - the constant 0x5C41BC
    // the left edge
    unsigned char* p = Gfx_PacketNext;
    AH_CALL(Gpu_SetPolyFT4)(p);
    AH_CALL(Gpu_SetSemiTrans)(p, 1);
    p[0x14] = 0;
    SetFloat(p + 8, fx);
    p[0x24] = 2;
    p[0x25] = 0;
    p[0x34] = 0;
    SetFloat(p + 0x1C, fy);
    SetFloat(p + 0x18, fx2);
    SetFloat(p + 0x28, fx);
    SetFloat(p + 0x38, fx2);
    SetFloat(p + 0x3C, fyh);
    p[0x45] = hb;
    p[0x35] = static_cast<unsigned char>(hb - 3);
    SetFloat(p + 0xC, fy2);
    p[0x44] = 2;
    p[4] = shade;
    SetFloat(p + 0x2C, fyh3);
    p[5] = shade;
    p[6] = shade;
    p[0x15] = 2;
    SetWord(p + 0x16, static_cast<unsigned>(AH_CALL(Gpu_GetClut)(clut_x, 0x1E1)));
    SetWord(p + 0x26, tpage);
    AH_CALL(Gfx_CommitPrim)(1, 0x48);
    // the middle's left half
    const int ww = static_cast<int>(w1 & 0xFFFF);
    const int half = (ww - 4) >> 1;
    const int odd = static_cast<int>(w1 & 1);
    const int hs = static_cast<short>(half);
    const auto hu = static_cast<unsigned char>(half);
    p = Gfx_PacketNext;
    AH_CALL(Gpu_SetPolyFT4)(p);
    AH_CALL(Gpu_SetSemiTrans)(p, 1);
    const float fxm = static_cast<float>(hs + xs + 2);
    SetFloat(p + 8, fx2);
    SetFloat(p + 0xC, fy);
    SetFloat(p + 0x1C, fy);
    SetFloat(p + 0x28, fx2);
    SetFloat(p + 0x3C, fyh);
    SetFloat(p + 0x18, fxm);
    SetFloat(p + 0x38, fxm);
    SetFloat(p + 0x2C, fyh);
    p[0x14] = 0;
    p[0x15] = 0;
    p[0x24] = hu;
    p[0x25] = 0;
    p[0x34] = 0;
    p[0x35] = hb;
    p[0x44] = hu;
    p[0x45] = hb;
    p[4] = shade;
    p[5] = shade;
    p[6] = shade;
    SetWord(p + 0x16, static_cast<unsigned>(AH_CALL(Gpu_GetClut)(clut_x, 0x1E1)));
    SetWord(p + 0x26, tpage);
    AH_CALL(Gfx_CommitPrim)(1, 0x48);
    // the middle's right half
    p = Gfx_PacketNext;
    AH_CALL(Gpu_SetPolyFT4)(p);
    AH_CALL(Gpu_SetSemiTrans)(p, 1);
    const float fxr = static_cast<float>(xs + odd + hs * 2 + 2);
    const auto ur = static_cast<unsigned char>(odd + hu);
    SetFloat(p + 8, fxm);
    SetFloat(p + 0xC, fy);
    p[0x14] = 0;
    SetFloat(p + 0x1C, fy);
    SetFloat(p + 0x28, fxm);
    SetFloat(p + 0x2C, fyh);
    SetFloat(p + 0x18, fxr);
    SetFloat(p + 0x38, fxr);
    SetFloat(p + 0x3C, fyh);
    p[0x15] = 0;
    p[0x24] = ur;
    p[0x25] = 0;
    p[0x34] = 0;
    p[0x35] = hb;
    p[0x44] = ur;
    p[0x45] = hb;
    p[4] = shade;
    p[5] = shade;
    p[6] = shade;
    SetWord(p + 0x16, static_cast<unsigned>(AH_CALL(Gpu_GetClut)(clut_x, 0x1E1)));
    SetWord(p + 0x26, tpage);
    AH_CALL(Gfx_CommitPrim)(1, 0x48);
    // the right edge
    p = Gfx_PacketNext;
    AH_CALL(Gpu_SetPolyFT4)(p);
    AH_CALL(Gpu_SetSemiTrans)(p, 1);
    const int xr = xs + ww;
    const float fxe = static_cast<float>(xr);
    const float fxe2 = static_cast<float>(xr - 2);
    SetFloat(p + 0xC, fy);
    SetFloat(p + 8, fxe2);
    SetFloat(p + 0x18, fxe);
    SetFloat(p + 0x28, fxe2);
    p[4] = shade;
    p[5] = shade;
    p[6] = shade;
    SetFloat(p + 0x38, fxe);
    SetFloat(p + 0x3C, fyh3);
    SetFloat(p + 0x2C, fyh - 1.0f);   // - the constant 0x5C41B8
    SetFloat(p + 0x1C, fy2);
    p[0x14] = 0;
    p[0x15] = 0;
    p[0x24] = 2;
    p[0x34] = 0;
    p[0x44] = 2;
    p[0x25] = 2;
    p[0x35] = static_cast<unsigned char>(hb - 1);
    p[0x45] = static_cast<unsigned char>(hb - 3);
    SetWord(p + 0x16, static_cast<unsigned>(AH_CALL(Gpu_GetClut)(clut_x, 0x1E1)));
    SetWord(p + 0x26, tpage);
    AH_CALL(Gfx_CommitPrim)(1, 0x48);
    // the second draw mode, then the outline
    unsigned char* const rect1 = Gfx_PacketNext;
    Gfx_PacketNext = rect1 + 8;
    SetWord(rect1, 0);
    SetWord(rect1 + 2, 0);
    SetWord(rect1 + 4, 0x100);
    SetWord(rect1 + 6, 0x100);
    AH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage,
                             static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(rect1)));
    AH_CALL(Gfx_CommitPrim)(1, 0xC);
    AH_CALL(Menu_DrawOutline)(static_cast<int>(static_cast<std::uint32_t>(x) + 2),
                              static_cast<int>(static_cast<std::uint32_t>(y) + 2), static_cast<int>(w1 - 5),
                              static_cast<int>(h1 - 5), flag);
}

void AreaW1f_Inject() {
    if (bof3::WantsShadow("area_w1f")) area_w1f::SelfTest();
    BOF3_INJECT(Area68_ChoiceMessageA);
    BOF3_INJECT(Area68_ChoiceMessageB);
    BOF3_INJECT(Area68_ChoiceAskCount);
    BOF3_INJECT(Area68_ChoiceAnswer84);
    BOF3_INJECT(Area68_SpawnKind4AtMember1);
    BOF3_INJECT(Area68_SpawnKind4AtMember2);
    BOF3_INJECT(Area68_GlideRunA);
    BOF3_INJECT(Area68_GlideBegin10);
    BOF3_INJECT(Area68_GlideStepA);
    BOF3_INJECT(Area68_GlideRunB);
    BOF3_INJECT(Area68_GlideStepB);
    BOF3_INJECT(Area68_SpawnKind1AtMember1);
    BOF3_INJECT(Area68_SpawnKind1AtMember2);
    BOF3_INJECT(Area68_SpawnKind3AtMember1);
    BOF3_INJECT(Area68_SpawnKind3AtMember2);
    BOF3_INJECT(Area68_Trigger25);
    BOF3_INJECT(Area69_GlideRun);
    BOF3_INJECT(Area69_GlideBegin40);
    BOF3_INJECT(Area69_GlideStep);
    BOF3_INJECT(Area71_SpawnEffect3C);
    BOF3_INJECT(Area71_SpawnEffect25);
    BOF3_INJECT(Area72_PlaceRandomObject);
    BOF3_INJECT(Area73_PlaceRandomObject);
    BOF3_INJECT(Area74_ChoiceAsk92);
    BOF3_INJECT(Area74_ChoiceAnswer94);
    BOF3_INJECT(Area74_PushToggleRow7);
    BOF3_INJECT(Area74_ClearMemberBit0);
    BOF3_INJECT(Area74_Trigger28);
    BOF3_INJECT(Area75_DropIn0);
    BOF3_INJECT(Area75_ArmTail30);
    BOF3_INJECT(Area75_MarkObjectB);
    BOF3_INJECT(Area75_MarkObjectA);
    BOF3_INJECT(Area75_ShowEffects);
    BOF3_INJECT(Area75_PoseObject5);
    BOF3_INJECT(Area75_SpawnFive);
    BOF3_INJECT(Area75_FlyRun);
    BOF3_INJECT(Area75_FlyBegin);
    BOF3_INJECT(Area75_FlyStep);
    BOF3_INJECT(Area75_DrainEffects);
    BOF3_INJECT(Area75_FollowRun);
    BOF3_INJECT(Area75_FollowBegin);
    BOF3_INJECT(Area75_FollowStep);
    BOF3_INJECT(Area75_FollowKind2Z);
    BOF3_INJECT(Area75_ChoiceStart14);
    BOF3_INJECT(Area75_ChoiceStartF);
    BOF3_INJECT(Area75_TailPresses);
    BOF3_INJECT(Area75_StepHook);
    BOF3_INJECT(Area75_Trigger41);
    BOF3_INJECT(Area75_ResetPresses);
    BOF3_INJECT(Area75_PhaseRun);
    BOF3_INJECT(Area75_PhaseDeal);
    BOF3_INJECT(Area75_PhasePlay);
    BOF3_INJECT(Area75_PhaseMissed);
    BOF3_INJECT(Area75_PhaseReached);
    BOF3_INJECT(Area75_OtherPress);
    BOF3_INJECT(Area75_PlayerPress);
    BOF3_INJECT(Area75_EarlyPress);
    BOF3_INJECT(Area75_DrawCounters);
    BOF3_INJECT(Area75_DrawWindow);
}

// World 0's areas 27..29 and 32..37: the PSX's BIN/WORLD00/AREA027..037.EMI
// compiled into the exe at 0x403400..0x4053B0 (Area_Descriptors entries 27,
// 28, 29, 32..37; areas 30 and 31 have no code). Round ten, group AR0C: the
// 52 functions of the band not already ours, each read to its last
// instruction with capstone (2026-09-27) and taken through the area harness
// (area_harness.h). Round eight's DA took the band's 20 world-map functions
// (worldmap_area.cpp) and round seven's world_map.cpp the map's frame and
// draws; they are called here by name. docs/area_w0c.md.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// five dispatchers through an area's .data state table abort past the table
// where the original would call whatever the next dwords hold (the owner's
// rule for an unchecked index, round9 doc section 6; no route reaches it).
// Every call goes through the harness (AH_CALL / AH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies.
#include "game/area_w0c.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w0c_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w0c::at;
using area_harness::Mem;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

using Handler = void (__cdecl*)();

unsigned char& B(std::uint32_t address) { return *Mem(address); }
std::int16_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }
void AddLong(unsigned char* p, std::int32_t v) { SetLong(p, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(p)) + static_cast<std::uint32_t>(v))); }
// Effect_Objects record `slot` (0x80 bytes).
unsigned char* EffectAt(unsigned slot) { return Effect_Objects + slot * at::kEffectStride; }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is the next table, or data).
Handler StateEntry(const char* who, std::uint32_t table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + index * 4u)))));
}

// Choice handlers read the message box's answer; most set the message word
// to 0xFFFF (no new message) first.
signed char Answer() { return static_cast<signed char>(B(at::kChoiceAnswer)); }
void SetMessage(unsigned id) { SetWord(Mem(at::kMessage), id); }

// Effect_FindFree's slot to the running object's +0xB, then (0xFF: none) a
// new effect record of `kind` at that slot: +0 = 1 and +5 = the kind, the
// slot re-read from the object for each store as the originals re-read it.
bool SpawnFromCurrent(unsigned char kind) {
    Sprite_Current[0xB] = AH_CALL(Effect_FindFree)();
    unsigned char* const cur = Sprite_Current;
    if (cur[0xB] == 0xFF) return false;
    EffectAt(cur[0xB])[0] = 1;
    EffectAt(cur[0xB])[5] = kind;
    return true;
}

// Area 32's two effects of kind 0x50 (0x403AF0, 0x403C40): the running
// object's +8 to the effect's +6, its position to the effect's +0x34..+0x3F
// raised by `lift` in +0x38 and by 0x100 in +0x3C's high word, and `timer`
// to +9.
void SpawnEffect50(std::int32_t lift, unsigned char timer) {
    if (!SpawnFromCurrent(0x50)) return;
    const unsigned char* const cur = Sprite_Current;
    EffectAt(cur[0xB])[6] = cur[8];
    SetLong(EffectAt(cur[0xB]) + 0x34, Long(cur + 0x34));
    SetLong(EffectAt(cur[0xB]) + 0x38, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(cur + 0x38)) + static_cast<std::uint32_t>(lift)));
    SetLong(EffectAt(cur[0xB]) + 0x3C, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(cur + 0x3C)) + 0x1000000u));
    EffectAt(cur[0xB])[9] = timer;
}

// Areas 28 and 34's effects at a fixed cell (0x403750, 0x404E00): +0x38 z,
// +0x34 x, +0 = 1, +5 the kind, (area 34) +1 = 6, and +0x3C the ground's
// elevation there, as a word, in the high half.
void SpawnAtCell(unsigned slot, std::int32_t x, std::int32_t z, unsigned char kind, bool state6) {
    unsigned char* const e = EffectAt(slot);
    SetLong(e + 0x38, z);
    SetLong(e + 0x34, x);
    e[0] = 1;
    e[5] = kind;
    if (state6) e[1] = 6;
    const auto ground = static_cast<std::int16_t>(AH_CALL(AreaMap_Elevation)(Long(e + 0x34), Long(e + 0x38)));
    SetLong(e + 0x3C, static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(ground)) << 16));
}

// Area 27's two mode-tail phases (0x403570 kind 1, 0x403660 kind 2): one
// machine by the s8 state 0x9039F4, 0..4, differing only in the story flag,
// Party_DropIn's entry, the counter value state 2 waits for, and the
// destination's x and flags.
struct DropInTail {
    unsigned flag, entry;
    unsigned char wait;
    std::int32_t x;
    unsigned flags;
};
void RunDropInTail(const DropInTail& t) {
    switch (static_cast<signed char>(B(at::kTailState))) {
    case 0:
        if (Field_Request == 2) return;
        AH_CALL(ScriptFlags_Set40)();
        AH_CALL(Flags_Set)(Mem(at::kStoryFlags), t.flag);
        B(at::kTailState) = 1;
        return;
    case 1:
        AH_CALL(Party_DropIn)(t.entry);
        B(at::kTailState) = 2;
        return;
    case 2:
        if (B(at::kCounter3) != t.wait) return;
        SetWord(Mem(at::kTailTimer), 0x10);
        B(at::kTailState) = 3;
        return;
    case 3: {
        const auto left = static_cast<std::uint16_t>(Word(Mem(at::kTailTimer)) - 1);
        SetWord(Mem(at::kTailTimer), left);
        if (left != 0) return;
        AH_CALL(Field_ChangeArea)(0x1B, t.x, 0x380000, t.flags);
        const auto counter = static_cast<unsigned char>(B(at::kCounter3) + 1);
        B(at::kTailState) = 4;
        B(at::kCounter3) = counter;
        return;
    }
    case 4:
        if (B(at::kCounter3) != 0x31) return;
        B(at::kCounter3) = 0;
        AH_CALL(ScriptFlags_Clear40)();
        AH_CALL(Flags_Clear)(Mem(at::kStoryFlags), t.flag);
        B(at::kTailKind) = 0;
        B(at::kTailState) = 0;
        return;
    default: return;   // above 4 or negative: `ja` to the ret
    }
}

// Area 35's three counter handlers (0x404EA0, 0x404EE0, 0x404F20): the
// active member's +0x80 bit 0 cleared, ScriptFlags_Set40, the four counters
// (n, 0, 0, 0), MoveScript_Var7 = 4 and its step 0x14.
void SetCounters(unsigned char n) {
    unsigned char* const member = Field_ActiveMember;
    member[0x80] &= 0xFE;
    AH_CALL(ScriptFlags_Set40)();
    B(at::kCounter0) = n;
    B(at::kCounter1) = 0;
    B(at::kCounter2) = 0;
    B(at::kCounter3) = 0;
    MoveScript_Var7 = 4;
    B(at::kVar7Step) = 0x14;
}

// Area 37's paired choice handlers: "ask" (answer 0: message `yes`; else
// message `no` and the mark 6) and "confirm" (answer not 0: message `no` and
// the mark 6; else 0xFFFF). The answer is tested as a byte.
void ChoiceAsk(unsigned yes, unsigned no) {
    if (B(at::kChoiceAnswer) == 0) {
        SetMessage(yes);
        return;
    }
    SetMessage(no);
    B(at::kAnswerMark) = 6;
}
void ChoiceConfirm(unsigned no) {
    if (B(at::kChoiceAnswer) != 0) {
        SetMessage(no);
        B(at::kAnswerMark) = 6;
        return;
    }
    SetMessage(0xFFFF);
}

// Area 37's four object triggers (0x405320..0x405380): ScriptFlags_Set40,
// the mode tail kind 4 with sub-kind `sub`; answer 0 in al.
unsigned char TriggerTail4(unsigned char sub) {
    AH_CALL(ScriptFlags_Set40)();
    B(at::kTailKind) = 4;
    B(at::kTailSub) = sub;
    return 0;
}

}  // namespace

// ===========================================================================
// Area 27 (descriptor 0x5ED848): seven choices, two handlers (the choices'
// last two), two mode-tail phases.
// ===========================================================================

// original 0x403400 (Area27_Choices[0]): message 0xFFFF; an answer with bit
// 0 clear arms the mode tail kind 1 (Area27_TailDropIn1) at state 0.
extern "C" void __cdecl Area27_ChoiceTail1(void) {
    const unsigned char answer = B(at::kChoiceAnswer);
    SetMessage(0xFFFF);
    if (answer & 1) return;
    B(at::kTailKind) = 1;
    B(at::kTailState) = 0;
}

// original 0x403430 (Area27_Choices[1]): the same for tail kind 2.
extern "C" void __cdecl Area27_ChoiceTail2(void) {
    const unsigned char answer = B(at::kChoiceAnswer);
    SetMessage(0xFFFF);
    if (answer & 1) return;
    B(at::kTailKind) = 2;
    B(at::kTailState) = 0;
}

// original 0x403460 (Area27_Choices[2]): message 0xFFFF; answer 0 sets
// counter 0 to 5, answer 1 to 6, any other nothing.
extern "C" void __cdecl Area27_ChoiceCounter5or6(void) {
    const signed char answer = Answer();
    SetMessage(0xFFFF);
    if (answer == 0) B(at::kCounter0) = 5;
    else if (answer == 1) B(at::kCounter0) = 6;
}

// original 0x403490 (Area27_Choices[3]): answer 0: 0x5B, 1: 0x5A.
extern "C" void __cdecl Area27_ChoiceCounter5Bor5A(void) {
    const signed char answer = Answer();
    SetMessage(0xFFFF);
    if (answer == 0) B(at::kCounter0) = 0x5B;
    else if (answer == 1) B(at::kCounter0) = 0x5A;
}

// original 0x4034C0 (Area27_Choices[4]): answer 0: 0x5C, 1: 0x5D.
extern "C" void __cdecl Area27_ChoiceCounter5Cor5D(void) {
    const signed char answer = Answer();
    SetMessage(0xFFFF);
    if (answer == 0) B(at::kCounter0) = 0x5C;
    else if (answer == 1) B(at::kCounter0) = 0x5D;
}

// original 0x4034F0 (Area27_Choices[5] = Area27_Handlers[0]; PSX
// 0x801F3710): a free effect slot (Effect_FindFree; 0xFF none) gets +0 = 1
// and the kind +5 = 0x2F.
extern "C" void __cdecl Area27_SpawnEffect2F(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    unsigned char* const e = EffectAt(slot);
    e[0] = 1;
    e[5] = 0x2F;
}

// original 0x403520 (Area27_Choices[6] = Area27_Handlers[1]; PSX
// 0x801F376C): six map cells zeroed, x 0x27 and 0x28 of rows 0x25..0x27.
extern "C" void __cdecl Area27_ClearCells(void) {
    AH_CALL(AreaMap_SetByte)(0x27, 0x25, 0);
    AH_CALL(AreaMap_SetByte)(0x27, 0x26, 0);
    AH_CALL(AreaMap_SetByte)(0x27, 0x27, 0);
    AH_CALL(AreaMap_SetByte)(0x28, 0x25, 0);
    AH_CALL(AreaMap_SetByte)(0x28, 0x26, 0);
    AH_CALL(AreaMap_SetByte)(0x28, 0x27, 0);
}

// original 0x403570 (Field_ModeTailKinds[1], armed by Area27_ChoiceTail1's
// register store): state 0 waits for Field_Request to leave 2, then
// ScriptFlags_Set40 and story flag 1; 1 Party_DropIn(1); 2 waits for counter
// 3 to reach 0x20, then a 0x10-frame timer; 3 counts it down, then
// Field_ChangeArea(0x1B, 0x4B0000, 0x380000, 0x8F) and counter 3 + 1; 4
// waits for counter 3 to reach 0x31, then clears it, ScriptFlags_Clear40,
// story flag 1 cleared, the tail disarmed. A 5-entry jump table in the body.
extern "C" void __cdecl Area27_TailDropIn1(void) { RunDropInTail({1, 1, 0x20, 0x4B0000, 0x8F}); }

// original 0x403660 (Field_ModeTailKinds[2], armed by Area27_ChoiceTail2):
// the same machine with story flag 0, Party_DropIn(2), counter 3 at 0x10, and
// Field_ChangeArea(0x1B, 0x260000, 0x380000, 0x90).
extern "C" void __cdecl Area27_TailDropIn2(void) { RunDropInTail({0, 2, 0x10, 0x260000, 0x90}); }

// ===========================================================================
// Area 28 (descriptor 0x5EE1A0): one handler of its own (handlers 0 and 1 are
// the shared `ret` 0x437CC0).
// ===========================================================================

// original 0x403750 (Area28_Handlers[2]; PSX 0x801F3424): a free effect
// slot gets kind 0x40 at cell (0x15, 0x5B), on the ground.
extern "C" void __cdecl Area28_SpawnEffect40(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    SpawnAtCell(slot, 0x150000, 0x5B0000, 0x40, false);
}

// ===========================================================================
// Area 32 (descriptor 0x5EF2C8): eight choices, six handlers (choices 2..7),
// two two-state machines through Area32_StatesA / Area32_StatesB.
// ===========================================================================

// original 0x403880 (Area32_Choices[0]): message 0xFFFF; answer 1 plays
// animation 0x80 on the leader (Sprite_Current made ObjTrio for the call,
// then put back).
extern "C" void __cdecl Area32_ChoiceLeaderAnim(void) {
    const unsigned char answer = B(at::kChoiceAnswer);
    SetMessage(0xFFFF);
    if (answer != 1) return;
    unsigned char* const kept = Sprite_Current;
    Sprite_Current = ObjTrio;
    AH_CALL(Sprite_SetAnimation)(0x80);
    Sprite_Current = kept;
}

// original 0x4038C0 (Area32_Choices[2] = Area32_Handlers[0]; PSX
// 0x801F36E4): Area32_StatesA by Sprite_Current[4].
extern "C" void __cdecl Area32_RunA(void) {
    StateEntry("Area32_RunA", at::kArea32StatesA, at::kArea32States, Sprite_Current[4])();
}

// original 0x4038E0 (Area32_StatesA[0]): Sprite_ShadeFadeBegin; the object's
// +0x5C = 1, its x +0x34 moved by its step +0xC, state 1; the leader's word
// +0x12E less 2.
extern "C" void __cdecl Area32_ShadeStart(void) {
    AH_CALL(Sprite_ShadeFadeBegin)();
    unsigned char* const cur = Sprite_Current;
    cur[0x5C] = 1;
    AddLong(cur + 0x34, Long(cur + 0xC));
    cur[4] = 1;
    AddWord(Field_State + 0x12E, -2);
}

// original 0x403920 (Area32_StatesA[1]): Sprite_ShadeLower(8); while it
// answers 0, the leader's +0x12E less 2 and the object moved by its step;
// once it answers, state 0.
extern "C" void __cdecl Area32_ShadeStep(void) {
    if (AH_CALL(Sprite_ShadeLower)(8) != 0) {
        Sprite_Current[4] = 0;
        return;
    }
    AddWord(Field_State + 0x12E, -2);
    unsigned char* const cur = Sprite_Current;
    AddLong(cur + 0x34, Long(cur + 0xC));
}

// original 0x403960 (Area32_Choices[3] = Area32_Handlers[1]; PSX
// 0x801F3800): Area32_StatesB by Sprite_Current[4].
extern "C" void __cdecl Area32_RunB(void) {
    StateEntry("Area32_RunB", at::kArea32StatesB, at::kArea32States, Sprite_Current[4])();
}

// original 0x403980 (Area32_StatesB[0]): Sprite_SetTint(the object, 0, 0,
// 0, 1)'s slot to the active member's +0x9F; the object's +0 bit 0x20 set
// and 0x40 cleared, +0x5C = 1, the tint bytes +0x5F / +0x5E / +0x5D = 0x80,
// step +0xC = 0x2000 and x +0x34 moved by it, state 1; the active member's
// word +0x8A less 2.
extern "C" void __cdecl Area32_TintStart(void) {
    const unsigned char tint = AH_CALL(Sprite_SetTint)(Sprite_Current, 0, 0, 0, 1);
    Field_ActiveMember[0x9F] = tint;
    unsigned char* const cur = Sprite_Current;
    cur[0] |= 0x20;
    cur[0] &= 0xBF;
    cur[0x5C] = 1;
    cur[0x5F] = 0x80;
    cur[0x5E] = 0x80;
    cur[0x5D] = 0x80;
    SetLong(cur + 0xC, 0x2000);
    AddLong(cur + 0x34, Long(cur + 0xC));
    cur[4] = 1;
    AddWord(Field_ActiveMember + 0x8A, -2);
}

// original 0x403A20 (Area32_StatesB[1]): each tint byte +0x5D, +0x5E, +0x5F
// below 0xC0 as a signed byte gains 4; when all three are exactly 0xC0,
// Tint_Release(the active member's +0x9F), the object's bit 0x20 cleared,
// +0x5C and the tint bytes 0, state 0, the active member's +0x84 = 2;
// otherwise the object moves by its step and the member's +0x8A less 2.
extern "C" void __cdecl Area32_TintStep(void) {
    unsigned char* cur = Sprite_Current;
    for (unsigned off = 0x5D; off <= 0x5F; ++off)
        if (static_cast<signed char>(cur[off]) < static_cast<signed char>(0xC0)) cur[off] = static_cast<unsigned char>(cur[off] + 4);
    if (cur[0x5D] == 0xC0 && cur[0x5E] == 0xC0 && cur[0x5F] == 0xC0) {
        AH_CALL(Tint_Release)(Field_ActiveMember[0x9F]);
        cur = Sprite_Current;
        cur[0] &= 0xDF;
        cur[0x5C] = 0;
        cur[0x5F] = 0;
        cur[0x5E] = 0;
        cur[0x5D] = 0;
        cur[4] = 0;
        Field_ActiveMember[0x84] = 2;
        return;
    }
    AddLong(cur + 0x34, Long(cur + 0xC));
    AddWord(Field_ActiveMember + 0x8A, -2);
}

// original 0x403AF0 (Area32_Choices[4] = Area32_Handlers[2]; PSX
// 0x801F3A78): an effect of kind 0x50 at the object, 0x4000 up, timer 0x78.
extern "C" void __cdecl Area32_SpawnEffect50Near(void) { SpawnEffect50(0x4000, 0x78); }

// original 0x403B90 (Area32_Choices[5] = Area32_Handlers[3]; PSX
// 0x801F3C18): an effect of kind 0x4F whose +0xB is the active member's
// index, (Field_ActiveMember - Sprite_Objects) / 0xA4 as a signed division.
extern "C" void __cdecl Area32_SpawnEffect4F(void) {
    Sprite_Current[0xB] = AH_CALL(Effect_FindFree)();
    unsigned char* const cur = Sprite_Current;
    if (cur[0xB] == 0xFF) return;
    const auto from = static_cast<std::int32_t>(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(Field_ActiveMember)) - at::kSpriteObjects);
    EffectAt(cur[0xB])[0] = 1;
    EffectAt(cur[0xB])[5] = 0x4F;
    EffectAt(cur[0xB])[0xB] = static_cast<unsigned char>(from / 0xA4);
}

// original 0x403C00 (Area32_Choices[6] = Area32_Handlers[4]; PSX
// 0x801F3D28): an effect of kind 0x4E.
extern "C" void __cdecl Area32_SpawnEffect4E(void) { SpawnFromCurrent(0x4E); }

// original 0x403C40 (Area32_Choices[7] = Area32_Handlers[5]; PSX
// 0x801F3DCC): an effect of kind 0x50 at the object, 0x8000 up, timer 0x3C.
extern "C" void __cdecl Area32_SpawnEffect50Far(void) { SpawnEffect50(0x8000, 0x3C); }

// ===========================================================================
// Area 33 (descriptor 0x5EF4D8, the Yraall world map): world-map record 1's
// slots +8 (effect kind 0x16) and +4 (effect kind 0xE), and their starts.
// The rest of the area is round eight's (worldmap_area.cpp).
// ===========================================================================

// original 0x404680 (WorldMap_Records[1] +8): WorldMap33_Record08States by
// Sprite_Current[1] - 0x4253C0 (shared by ten world maps), Area33_Record08Start,
// 0x40C490 (shared).
extern "C" void __cdecl Area33_Record08Run(void) {
    StateEntry("Area33_Record08Run", at::kRecord08States, at::kRecord08StateCount, Sprite_Current[1])();
}

// original 0x4046A0 (WorldMap33_Record08States[1]): animation bank 0x46;
// +0x48, +0x24 = 0, +0x29 = 5; the step words of WorldMap33_Record08Steps by
// the direction +8 to +0xC / +0x10 (sign-extended); the leader's position
// +0x34 / +0x38; for a variant +6 not 0, the words +0x36 / +0x3A drawn back
// by the step >> 13 and nudged 2 along one axis (+6 == 1: forward, else
// back; the axis +0x36 when +0xC is 0); always drawn back by the step >> 9;
// +0xB = 0, state + 1; the animation and +0x2A from WorldMap33_Record08Anims;
// then Sprite_UpdateScreen. The direction indexes both tables unchecked, as
// the original's (reads on through .data).
extern "C" void __cdecl Area33_Record08Start(void) {
    AH_CALL(Sprite_SetAnimationBank)(0x46);
    unsigned char* cur = Sprite_Current;
    cur[0x48] = 0;
    cur[0x24] = 0;
    cur[0x29] = 5;
    const auto StepX = [&] { return S16(Mem(at::kRecord08Steps + cur[8] * 4u)); };
    const auto StepZ = [&] { return S16(Mem(at::kRecord08Steps + 2 + cur[8] * 4u)); };
    SetLong(cur + 0xC, StepX());
    SetLong(cur + 0x10, StepZ());
    SetLong(cur + 0x34, Long(Mem(at::kLeaderX)));
    SetLong(cur + 0x38, Long(Mem(at::kLeaderZ)));
    if (cur[6] != 0) {
        AddWord(cur + 0x36, -static_cast<std::int16_t>(StepX() >> 13));
        AddWord(cur + 0x3A, -static_cast<std::int16_t>(StepZ() >> 13));
        const int nudge = cur[6] == 1 ? 2 : -2;
        if (Long(cur + 0xC) == 0) AddWord(cur + 0x36, nudge);
        else AddWord(cur + 0x3A, nudge);
    }
    AddWord(cur + 0x36, -static_cast<std::int16_t>(StepX() >> 9));
    AddWord(cur + 0x3A, -static_cast<std::int16_t>(StepZ() >> 9));
    cur[0xB] = 0;
    cur[1] = static_cast<unsigned char>(cur[1] + 1);
    AH_CALL(Sprite_SetAnimation)(B(at::kRecord08Anims + cur[8] * 2u));
    cur = Sprite_Current;
    cur[0x2A] = B(at::kRecord08Anims + 1 + cur[8] * 2u);
    AH_CALL(Sprite_UpdateScreen)();
}

// original 0x404800 (WorldMap_Records[1] +4): WorldMap33_Record04States by
// Sprite_Current[1] - Area33_Record04Start, 0x408990 (shared by ten world
// maps).
extern "C" void __cdecl Area33_Record04Run(void) {
    StateEntry("Area33_Record04Run", at::kRecord04States, at::kRecord04StateCount, Sprite_Current[1])();
}

// original 0x404820 (WorldMap33_Record04States[0]): with CharacterRecords
// record 0's byte +9 at 9, or Field_StatusBits bit 0, Effect_Release.
// Otherwise WorldMap_RecordIndex (its answer unused), animation bank 0x205,
// +0x48, +0x24, +0x2A and the tint bytes 0; the map cell of
// WorldMap33_Record04Cells[+0xB] (x, z) set to 0xA0 (AreaMap_Bytes + z *
// the header's width byte + x, unchecked); Sprite_SetAnimation(0); state 1.
extern "C" void __cdecl Area33_Record04Start(void) {
    if (B(at::kRecord0Byte9) == 9 || (Field_StatusBits & 1) != 0) {
        AH_CALL(Effect_Release)();
        return;
    }
    AH_CALL(WorldMap_RecordIndex)();
    AH_CALL(Sprite_SetAnimationBank)(0x205);
    unsigned char* const cur = Sprite_Current;
    cur[0x48] = 0;
    cur[0x24] = 0;
    cur[0x2A] = 0;
    cur[0x5D] = 0;
    cur[0x5E] = 0;
    cur[0x5F] = 0;
    const unsigned width = AreaMap_Header[0];
    const std::uint32_t cell = at::kRecord04Cells + cur[0xB] * 4u;
    const unsigned z = B(cell + 1), x = B(cell);
    const_cast<unsigned char*>(AreaMap_Bytes)[z * width + x] = 0xA0;
    AH_CALL(Sprite_SetAnimation)(0);
    Sprite_Current[1] = 1;
}

// ===========================================================================
// Area 34 (descriptor 0x5F01A0): three choices, two handlers (choices 1 and
// 2), an object trigger.
// ===========================================================================

// original 0x404D50 (Area34_Choices[0]): message 0xFFFF; answer 0 or 1 arms
// the mode tail kind 5 with sub-kind 5 or 4 and sets story flag 0x1E. Then,
// the answer (re-read) not 2, Cond_ByteFA 8 and flag 3 of the chapter's row
// (0x929ED0) clear: the tail disarmed, ScriptFlags_Set40, Party_DropIn(2),
// MoveScript_Var7 = 2 with step 0.
extern "C" void __cdecl Area34_ChoiceTail5(void) {
    const signed char answer = Answer();
    SetMessage(0xFFFF);
    if (answer == 0 || answer == 1) {
        B(at::kTailSub) = answer == 0 ? 5 : 4;
        B(at::kTailKind) = 5;
        AH_CALL(Flags_Set)(Mem(at::kStoryFlags), 0x1E);
    }
    if (B(at::kChoiceAnswer) == 2) return;
    if (static_cast<unsigned char>(Cond_ByteFA) != 8) return;
    const auto row = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(at::kFlagRow)))));
    if (AH_CALL(Flags_Test)(row, 3) != 0) return;
    B(at::kTailKind) = 0;
    AH_CALL(ScriptFlags_Set40)();
    AH_CALL(Party_DropIn)(2);
    MoveScript_Var7 = 2;
    B(at::kVar7Step) = 0;
}

// original 0x404DE0 (Area34_Choices[1] = Area34_Handlers[0]; PSX
// 0x801F2CDC): unless the leader's +0x89 is 7, the running script's position
// word (MoveScript_Object +0xA) moves on 0x15.
extern "C" void __cdecl Area34_SkipScript(void) {
    if (Field_State[0x89] == 7) return;
    AddWord(MoveScript_Object + 0xA, 0x15);
}

// original 0x404E00 (Area34_Choices[2] = Area34_Handlers[1]; PSX
// 0x801F2D1C): the slot to Sprite_Current[0xB]; an effect of kind 0x51,
// state 6, at cell (0x36, 0x42) on the ground.
extern "C" void __cdecl Area34_SpawnEffect51(void) {
    Sprite_Current[0xB] = AH_CALL(Effect_FindFree)();
    const unsigned char slot = Sprite_Current[0xB];
    if (slot == 0xFF) return;
    SpawnAtCell(slot, 0x360000, 0x420000, 0x51, true);
}

// original 0x404E60 (Field_ObjectTriggers[51], (object, flags) ignored):
// story flag 0x6D set; when flags 0x6D..0x70 are all set, flag 0x71 too.
// Answers 0 in al.
extern "C" unsigned char __cdecl Area34_Trigger51(unsigned char*, unsigned char*) {
    AH_CALL(Flags_Set)(Mem(at::kStoryFlags), 0x6D);
    for (unsigned flag = 0x6D; flag <= 0x70; ++flag)
        if (!AH_CALL(Flags_Test)(Mem(at::kStoryFlags), flag)) return 0;
    AH_CALL(Flags_Set)(Mem(at::kStoryFlags), 0x71);
    return 0;
}

// ===========================================================================
// Area 35 (descriptor 0x5F0330): four handlers.
// ===========================================================================

// originals 0x404EA0, 0x404EE0, 0x404F20 (Area35_Handlers[0..2]; PSX
// 0x801F2C04, 0x801F2C74, 0x801F2CE4): counter 0 to 1, 2, 3 (SetCounters).
extern "C" void __cdecl Area35_SetCounter1(void) { SetCounters(1); }
extern "C" void __cdecl Area35_SetCounter2(void) { SetCounters(2); }
extern "C" void __cdecl Area35_SetCounter3(void) { SetCounters(3); }

// original 0x404F60 (Area35_Handlers[3]; PSX 0x801F2D54): the active
// member's +0x80 bit 0 cleared and the running object's +0 = 0.
extern "C" void __cdecl Area35_HideObject(void) {
    Field_ActiveMember[0x80] &= 0xFE;
    Sprite_Current[0] = 0;
}

// ===========================================================================
// Area 36 (descriptor 0x5F0540): the step hook, and effect kind 0xB5's two
// states (its handlers are shared bodies of other groups).
// ===========================================================================

// original 0x404F80 (Area_StepHook's case for area 36, `(x, z)` answering
// in al): with Cond_ByteFD 2 and story flag 0x33, a step whose high words
// fall in x 0x46..0x48 and z 0x23..0x25 while the leader's byte +8 is 0, 6 or
// 7: counter 0 = 0, Party_DropIn(0), answer 1. Otherwise 0.
extern "C" int __cdecl Area36_StepHook(long x, long z) {
    if (Cond_ByteFD != 2) return 0;
    if (!AH_CALL(Flags_Test)(Mem(at::kStoryFlags), 0x33)) return 0;
    if (static_cast<std::uint16_t>((static_cast<std::uint32_t>(x) >> 16) - 0x46) >= 3) return 0;
    if (static_cast<std::uint16_t>((static_cast<std::uint32_t>(z) >> 16) - 0x23) >= 3) return 0;
    const unsigned char pose = B(at::kLeaderByte8);
    if (pose != 0 && pose != 7 && pose != 6) return 0;
    B(at::kCounter0) = 0;
    AH_CALL(Party_DropIn)(0);
    return 1;
}

// original 0x404FE0 (Effect_KindHandlers entry 0xB5): Area36_EffectStates by
// Sprite_Current[1] - 0x40B4F0 (shared by seven areas), Area36_EffectStep.
extern "C" void __cdecl Area36_EffectRun(void) {
    StateEntry("Area36_EffectRun", at::kArea36EffectStates, at::kArea36EffectStateCount, Sprite_Current[1])();
}

// original 0x405000 (Area36_EffectStates[1]): the object's position
// dwords +0x34, +0x38, +0x3C copied to the stack and handed to 0x4220D0.
extern "C" void __cdecl Area36_EffectStep(void) {
    const unsigned char* const cur = Sprite_Current;
    std::int32_t position[3] = {Long(cur + 0x34), Long(cur + 0x38), Long(cur + 0x3C)};
    AH_AT(void (__cdecl*)(std::int32_t*), at::kPositionHook)(position);
}

// ===========================================================================
// Area 37 (descriptor 0x5F14A0): 22 choices (six of them shared bodies of
// another group's, several named twice), one handler, a mode-tail phase,
// four object triggers.
// ===========================================================================

// originals 0x405030 / 0x405060 (Area37_Choices[0] / [3], [4]), 0x405090 /
// 0x4050C0 ([5] / [8], [9]), 0x4050F0 / 0x405120 ([10] / [13], [14]),
// 0x405150 / 0x405180 ([15] / [18], [19]): ChoiceAsk / ChoiceConfirm.
extern "C" void __cdecl Area37_ChoiceAsk5E(void) { ChoiceAsk(0x5E, 0x60); }
extern "C" void __cdecl Area37_ChoiceConfirm60(void) { ChoiceConfirm(0x60); }
extern "C" void __cdecl Area37_ChoiceAsk72(void) { ChoiceAsk(0x72, 0x74); }
extern "C" void __cdecl Area37_ChoiceConfirm74(void) { ChoiceConfirm(0x74); }
extern "C" void __cdecl Area37_ChoiceAsk86(void) { ChoiceAsk(0x86, 0x88); }
extern "C" void __cdecl Area37_ChoiceConfirm88(void) { ChoiceConfirm(0x88); }
extern "C" void __cdecl Area37_ChoiceAsk9A(void) { ChoiceAsk(0x9A, 0x9C); }
extern "C" void __cdecl Area37_ChoiceConfirm9C(void) { ChoiceConfirm(0x9C); }

// original 0x4051B0 (Area37_Choices[20]): answer 0: message 0xFFFF, counter
// 3 = 0x64, Party_DropIn(3); else message 0xBD, counter 3 = 0.
extern "C" void __cdecl Area37_ChoiceDropIn(void) {
    if (B(at::kChoiceAnswer) == 0) {
        SetMessage(0xFFFF);
        B(at::kCounter3) = 0x64;
        AH_CALL(Party_DropIn)(3);
        return;
    }
    SetMessage(0xBD);
    B(at::kCounter3) = 0;
}

// original 0x4051F0 (Area37_Choices[21]): answer 0: ScriptFlags_Set40 and
// the mode tail kind 0x2E (Area37_TailLeave); 1: counter 3 = 0x64; 2:
// counter 3 = 0x5F; message 0xFFFF for those three, the message word left
// alone for any other answer.
extern "C" void __cdecl Area37_ChoiceTail2E(void) {
    const unsigned char answer = B(at::kChoiceAnswer);
    if (answer == 0) {
        AH_CALL(ScriptFlags_Set40)();
        B(at::kTailKind) = 0x2E;
        SetMessage(0xFFFF);
    } else if (answer == 1) {
        SetMessage(0xFFFF);
        B(at::kCounter3) = 0x64;
    } else if (answer == 2) {
        SetMessage(0xFFFF);
        B(at::kCounter3) = 0x5F;
    }
}

// original 0x405240 (Field_ModeTailKinds[0x2E]): by the s8 state 0x9039F4,
// 0..3 (a 4-entry jump table in the body): 0 waits for Field_Request to
// leave 2, then story flag 0x72, Field_ChangeArea(0x25, 0x2C8000, 0x270000,
// 1), the pending kind 0xFE, state + 1; 1 waits for Field_Request 0, then
// message 0xB5 and Field_Request 2, state + 1; 2 waits for Field_Request to
// leave 2, then Transition_Start(1) and Draw_PassFlags 0x1F, state + 1; 3
// waits for MoveScript_WaitWordDA 0, then ScriptFlags_Clear40, the tail
// disarmed and counter 3 = 0. The state is re-read after each call.
extern "C" void __cdecl Area37_TailLeave(void) {
    switch (static_cast<signed char>(B(at::kTailState))) {
    case 0: {
        if (Field_Request == 2) return;
        AH_CALL(Flags_Set)(Mem(at::kStoryFlags), 0x72);
        AH_CALL(Field_ChangeArea)(0x25, 0x2C8000, 0x270000, 1);
        const auto next = static_cast<unsigned char>(B(at::kTailState) + 1);
        B(at::kPendingKind) = 0xFE;
        B(at::kTailState) = next;
        return;
    }
    case 1: {
        if (Field_Request != 0) return;
        AH_CALL(Msg_OpenScript)(0xB5);
        const auto next = static_cast<unsigned char>(B(at::kTailState) + 1);
        Field_Request = 2;
        B(at::kTailState) = next;
        return;
    }
    case 2: {
        if (Field_Request == 2) return;
        AH_CALL(Transition_Start)(1);
        const auto next = static_cast<unsigned char>(B(at::kTailState) + 1);
        Draw_PassFlags = 0x1F;
        B(at::kTailState) = next;
        return;
    }
    case 3:
        if (MoveScript_WaitWordDA != 0) return;
        AH_CALL(ScriptFlags_Clear40)();
        B(at::kTailKind) = 0;
        B(at::kTailState) = 0;
        B(at::kCounter3) = 0;
        return;
    default: return;   // above 3 or negative: `ja` to the ret
    }
}

// originals 0x405320, 0x405340, 0x405360, 0x405380 (Field_ObjectTriggers
// [45..48], (object, flags) ignored): the mode tail kind 4 with sub-kind
// 0xB, 0xC, 0xE, 0xD. Answer 0 in al.
extern "C" unsigned char __cdecl Area37_Trigger45(unsigned char*, unsigned char*) { return TriggerTail4(0xB); }
extern "C" unsigned char __cdecl Area37_Trigger46(unsigned char*, unsigned char*) { return TriggerTail4(0xC); }
extern "C" unsigned char __cdecl Area37_Trigger47(unsigned char*, unsigned char*) { return TriggerTail4(0xE); }
extern "C" unsigned char __cdecl Area37_Trigger48(unsigned char*, unsigned char*) { return TriggerTail4(0xD); }

// original 0x4053A0 (Area37_Handlers[0]; PSX 0x801F3B14): Field_ScriptFlags
// bit 3 toggled.
extern "C" void __cdecl Area37_ToggleScriptFlag8(void) { Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags ^ 8); }

void AreaW0c_Inject() {
    if (bof3::WantsShadow("area_w0c")) area_w0c::SelfTest();
    BOF3_INJECT(Area27_ChoiceTail1);
    BOF3_INJECT(Area27_ChoiceTail2);
    BOF3_INJECT(Area27_ChoiceCounter5or6);
    BOF3_INJECT(Area27_ChoiceCounter5Bor5A);
    BOF3_INJECT(Area27_ChoiceCounter5Cor5D);
    BOF3_INJECT(Area27_SpawnEffect2F);
    BOF3_INJECT(Area27_ClearCells);
    BOF3_INJECT(Area27_TailDropIn1);
    BOF3_INJECT(Area27_TailDropIn2);
    BOF3_INJECT(Area28_SpawnEffect40);
    BOF3_INJECT(Area32_ChoiceLeaderAnim);
    BOF3_INJECT(Area32_RunA);
    BOF3_INJECT(Area32_ShadeStart);
    BOF3_INJECT(Area32_ShadeStep);
    BOF3_INJECT(Area32_RunB);
    BOF3_INJECT(Area32_TintStart);
    BOF3_INJECT(Area32_TintStep);
    BOF3_INJECT(Area32_SpawnEffect50Near);
    BOF3_INJECT(Area32_SpawnEffect4F);
    BOF3_INJECT(Area32_SpawnEffect4E);
    BOF3_INJECT(Area32_SpawnEffect50Far);
    BOF3_INJECT(Area33_Record08Run);
    BOF3_INJECT(Area33_Record08Start);
    BOF3_INJECT(Area33_Record04Run);
    BOF3_INJECT(Area33_Record04Start);
    BOF3_INJECT(Area34_ChoiceTail5);
    BOF3_INJECT(Area34_SkipScript);
    BOF3_INJECT(Area34_SpawnEffect51);
    BOF3_INJECT(Area34_Trigger51);
    BOF3_INJECT(Area35_SetCounter1);
    BOF3_INJECT(Area35_SetCounter2);
    BOF3_INJECT(Area35_SetCounter3);
    BOF3_INJECT(Area35_HideObject);
    BOF3_INJECT(Area36_StepHook);
    BOF3_INJECT(Area36_EffectRun);
    BOF3_INJECT(Area36_EffectStep);
    BOF3_INJECT(Area37_ChoiceAsk5E);
    BOF3_INJECT(Area37_ChoiceConfirm60);
    BOF3_INJECT(Area37_ChoiceAsk72);
    BOF3_INJECT(Area37_ChoiceConfirm74);
    BOF3_INJECT(Area37_ChoiceAsk86);
    BOF3_INJECT(Area37_ChoiceConfirm88);
    BOF3_INJECT(Area37_ChoiceAsk9A);
    BOF3_INJECT(Area37_ChoiceConfirm9C);
    BOF3_INJECT(Area37_ChoiceDropIn);
    BOF3_INJECT(Area37_ChoiceTail2E);
    BOF3_INJECT(Area37_TailLeave);
    BOF3_INJECT(Area37_Trigger45);
    BOF3_INJECT(Area37_Trigger46);
    BOF3_INJECT(Area37_Trigger47);
    BOF3_INJECT(Area37_Trigger48);
    BOF3_INJECT(Area37_ToggleScriptFlag8);
}

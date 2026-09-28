// The boss set-ups' spawn helpers (0x4948E0..0x494A7D): six engine functions
// the boss band calls 57 times with a tag (tools/boss_rows.py's frontier:
// 0x494920 by 15 units, 0x4949D0 by 22, 0x4949F0 by 9, 0x494A60 by 7,
// 0x4948E0 and 0x494980 by 2 each). Each read to its last instruction with
// capstone (2026-09-28) and taken by round eleven group BH as a small engine
// module (docs/boss_h.md section 5), through the boss harness.
//
// A field actor here is one of the 30 field objects (Sprite_Objects,
// 0x7DEE80, stride 0xA4) whose type byte +6 is 7; its tag is the byte +0x9E.
// The battle keeps the field's objects, and a boss set-up finds the actors of
// its scene by tag to hide, show, pose or place them (Boss01's exit hook
// 0x437E10 copies two enemies' places and poses onto the actors tagged 6 and
// 7). What bit 0x40 of an actor's +0 means is not read here; the name says
// what the code does.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Three
// write through BossActor_Find's answer without testing it (Capcom's, the
// same code in all three); when no actor carries the tag the original writes
// near address 0 and faults. Ours aborts with a message there instead (the
// owner's rule for an unchecked index or pointer, round9 doc section 6); no
// route or fuzz reaches it, and docs/boss_h.md section 6 describes it.
#include "game/boss_spawn.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

constexpr U kObjects = 0x7DEE80;      // Sprite_Objects
constexpr U kObjectStride = 0xA4;
constexpr unsigned kObjectCount = 30;
constexpr U kEnemyData = 0x8C55C8;    // the area's eight enemy data records
constexpr U kEnemyDataStride = 0x8C;

unsigned char* ObjectAt(unsigned i) { return At(kObjects + i * kObjectStride); }
bool Tagged(const unsigned char* o, unsigned tag) { return o[6] == 7 && o[0x9E] == (tag & 0xFF); }

// The actor BossActor_Find answers, or a Fatal where the original would write
// through its null answer.
unsigned char* FoundOrAbort(const char* who, unsigned tag) {
    unsigned char* const p = BH_CALL(BossActor_Find)(tag);
    if (p == nullptr)
        bof3::Fatal("%s: no field actor carries tag %u - the original writes through the null pointer "
                    "BossActor_Find answers (docs/boss_h.md section 6)",
                    who, tag & 0xFF);
    return p;
}

}  // namespace

// original 0x4948E0: the index 0..7 of the first of the loaded area's eight
// enemy data records (0x8C55C8 + i * 0x8C) whose byte +0xC is the tag's low
// byte, or 0xFF. Answers in al (the original keeps the caller's upper eax).
extern "C" unsigned char __cdecl EnemyData_FindByTag(unsigned tag) {
    for (unsigned i = 0; i < 8; ++i)
        if (At(kEnemyData + i * kEnemyDataStride)[0xC] == (tag & 0xFF)) return static_cast<unsigned char>(i);
    return 0xFF;
}

// original 0x494920: the first field actor (Sprite_Objects 0..29, type +6 of
// 7) whose +0x9E is the tag's low byte, or null.
extern "C" unsigned char* __cdecl BossActor_Find(unsigned tag) {
    for (unsigned i = 0; i < kObjectCount; ++i)
        if (Tagged(ObjectAt(i), tag)) return ObjectAt(i);
    return nullptr;
}

// original 0x494980: BossActor_Find's search answering the index 0..29 in al,
// or 0xFF (the original keeps the caller's upper eax).
extern "C" unsigned char __cdecl BossActor_Index(unsigned tag) {
    for (unsigned i = 0; i < kObjectCount; ++i)
        if (Tagged(ObjectAt(i), tag)) return static_cast<unsigned char>(i);
    return 0xFF;
}

// original 0x4949D0: the tagged actor's +0 loses bit 0x40. BossActor_Find's
// answer is not tested (a Fatal here where the original faults).
extern "C" void __cdecl BossActor_ClearBit40(unsigned tag) {
    unsigned char* const p = FoundOrAbort("BossActor_ClearBit40", tag);
    p[0] &= 0xBF;
}

// original 0x4949F0: onto the tagged actor, by what's low byte - 0: from's
// pose (+0x4B the animation byte, the dwords +0x50 / +0x54, the words +0x58 /
// +0x5A) and +7 |= 0x20; 1: from's place (the dwords +0x34 / +0x38 / +0x3C);
// anything else: nothing, the actor looked for all the same. The answer is
// tested by neither copy (a Fatal here where the original faults); for any
// other `what` a missing actor is harmless in both.
extern "C" void __cdecl BossActor_CopyFrom(unsigned tag, const unsigned char* from, unsigned what) {
    unsigned char* const p = BH_CALL(BossActor_Find)(tag);
    const unsigned mode = what & 0xFF;
    if (mode > 1) return;
    if (p == nullptr)
        bof3::Fatal("BossActor_CopyFrom: no field actor carries tag %u - the original writes through the null pointer "
                    "BossActor_Find answers (docs/boss_h.md section 6)",
                    tag & 0xFF);
    if (mode == 1) {
        SetLong(p + 0x34, Long(from + 0x34));
        SetLong(p + 0x38, Long(from + 0x38));
        SetLong(p + 0x3C, Long(from + 0x3C));
        return;
    }
    p[0x4B] = from[0x4B];
    SetLong(p + 0x50, Long(from + 0x50));
    SetLong(p + 0x54, Long(from + 0x54));
    SetWord(p + 0x58, Word(from + 0x58));
    SetWord(p + 0x5A, Word(from + 0x5A));
    p[7] |= 0x20;
}

// original 0x494A60: the tagged actor's bytes +0..+4 zeroed (a Fatal where
// the original faults on a missing actor).
extern "C" void __cdecl BossActor_Clear(unsigned tag) {
    unsigned char* const p = FoundOrAbort("BossActor_Clear", tag);
    for (unsigned i = 0; i < 5; ++i) p[i] = 0;
}

void BossSpawn_Inject() {
    if (bof3::WantsShadow("boss_spawn")) boss_spawn::SelfTest();
    BOF3_INJECT(EnemyData_FindByTag);
    BOF3_INJECT(BossActor_Find);
    BOF3_INJECT(BossActor_Index);
    BOF3_INJECT(BossActor_ClearBit40);
    BOF3_INJECT(BossActor_CopyFrom);
    BOF3_INJECT(BossActor_Clear);
}

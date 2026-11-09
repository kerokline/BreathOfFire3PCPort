// World 4's areas 173 and 174: the PSX's BIN/WORLD04/AREA173.EMI and
// AREA174.EMI compiled into the exe at 0x428450..0x4292C0 (Area_Descriptors
// entries 173 and 174, descriptors 0x63FF68 and 0x641650), and six choice
// bodies areas 175..185 share that the linker put at the band's end. Round
// ten, group AR4C: the band's 39 functions, none ours before, each read to its
// last instruction with capstone (2026-09-28) and taken through the area
// harness (area_harness.h). docs/area_w4c.md.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Reads
// by an unchecked index (the party records past the member count, the pose
// tables, the descriptor's script table) are kept; where the original writes
// through an effect slot past the 20 effect records or calls through its
// stack table past its two entries, ours aborts with a message instead (round
// nine section 6: no ledger entry). Every call goes through the harness
// (AH_CALL / AH_AT), so the start-up fuzz can stand recorders in for ours as
// for the originals' copies; the group's own callee (Area174_SetPose) is
// called the same way, so each function is fuzzed alone.
#include "game/area_w4c.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
#include "game/area_harness.h"
#include "game/area_w4c_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w4c::at;
using area_harness::Mem;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }
U Key(const void* p) { return Low32(p); }
void AddWord(unsigned char* p, unsigned v) { SetWord(p, Word(p) + v); }
// The movement script back (or on) by `delta` words: the running script
// object's u16 +0xA, the pointer read at the store.
void ScriptStep(unsigned delta) { AddWord(MoveScript_Object + 0xA, delta); }

// Effect record `slot` (Effect_Objects, 20 of 0x80 bytes). The handlers write
// through Effect_FindFree's slot, or one an earlier handler kept in the
// running object's +0xB, unchecked: past the 20 records the original writes
// into what follows them - ours aborts.
unsigned char* EffectAt(const char* who, unsigned slot) {
    if (slot >= at::kEffectCount)
        bof3::Fatal("%s: the effect slot %u is past the 20 effect records (the original writes 0x%X, past "
                    "Effect_Objects)",
                    who, slot, static_cast<unsigned>(::bof3::Img(Effect_Objects_addr) + slot * at::kEffectStride));
    return Mem(::bof3::Img(Effect_Objects_addr) + slot * at::kEffectStride);
}

// Field_ActiveMember's index among the field objects as the originals compute
// it: (the pointer - Sprite_Objects) / 0xA4, C's truncating signed division
// (imul 0x63E7063F, sar 6, plus the sign bit); only its low byte is kept.
unsigned char ActiveMemberIndex() {
    const auto diff = static_cast<std::int32_t>(Key(Field_ActiveMember) - ::bof3::Img(Sprite_Objects_addr));
    return static_cast<unsigned char>(diff / static_cast<std::int32_t>(at::kObjectStride));
}

// Areas 173 and 174's effect of kind 0x9D (handlers 5 and 6 of 173, 3 and 4
// of 174): +0 = 1, kind +5 = 0x9D, +6 = `sub`, +0xB = Field_ActiveMember's
// index byte, word +0x2E = `word2E`, word +0x30 = 0xA5.
void Effect9D(const char* who, unsigned slot, unsigned char sub, unsigned char index, unsigned word2E) {
    unsigned char* const e = EffectAt(who, slot);
    auto* const tE = rv::As<records::EffectObject>(e);
    e[0] = 1;
    e[5] = 0x9D;
    e[6] = sub;
    e[0xB] = index;
    tE->screen_x = word2E;
    tE->screen_y = 0xA5;
}

}  // namespace

// ===========================================================================
// Area 173 (descriptor 0x63FF68): handlers 1..6 (handler 0, 0x42C8A0, lies
// outside the band), tail kind 38, the arrive hook, the init.
// ===========================================================================

// ===========================================================================
// Area 174 (descriptor 0x641650): handlers 0 and 2..21 (handler 1, 0x42D250,
// lies outside the band), choice 0 (= handler 21), no init.
// ===========================================================================

// ===========================================================================
// Choices areas 175..185 share (their +0x34 tables name the same six bodies,
// which the linker put in this band).
// ===========================================================================

void AreaW4c_Inject() {
    if (bof3::WantsShadow("area_w4c")) area_w4c::SelfTest();
}

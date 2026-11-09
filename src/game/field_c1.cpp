// Round twelve group FC1 (docs/field_c1.md): the field core's first half -
// analysis/round12_cut.tsv's 41 rows for FC1, each read to its last
// instruction with capstone (2026-09-29) and fuzzed through the scenario
// harness's field mode (scenario_harness.h, docs/scenario_harness.md section 7).
//
//   Config_DrawRowLabel        0x461800  the Config screen's row: box, label, hand, separator
//   EffectKind06_PlayOnce      0x469D10  EffectKind06_Ticks[5] / EffectKind19_Ticks[5]
//   EffectKind06_Fade          0x469D40  EffectKind06_Ticks[3] / EffectKind19_Ticks[3]
//   EffectKind19_Run           0x469DE0  Effect_KindHandlers[0x19]
//   EffectKind19_Tick          0x469E00  EffectKind19_States[1]
//   EffectKind04_HoldTick      0x469FB0  Effect_KindHandlers[4]
//   EffectKind31_Run           0x46A020  Effect_KindHandlers[0x31]
//   CameraZoom_Start / _Step / _End  0x46A040 / 0x46A070 / 0x46A0C0  CameraZoom_States 0..2
//   EffectKind32_Run           0x46A0E0  Effect_KindHandlers[0x32]
//   EffectKind32_Throw / _Arc / _Settle  0x46A100 / 0x46A1E0 / 0x46A290  EffectKind32_States 0..2
//   Effect_StateRelease        0x46A310  49 state-table cells: a tail jump to Effect_Release
//   EffectKind37_Start / _Play 0x46A340 / 0x46A390  EffectKind37_States 0, 1
//   EffectKind14_Start / _Hold 0x46A600 / 0x46A7F0  EffectKind14_States 0, 1
//   EffectKind3C_Start / _Hold 0x46A950 / 0x46AB30  EffectKind3C_States 0, 1
//   EffectKind17_Start .. _Rest 0x46ABD0 .. 0x46B340  EffectKind17_States 0..5 (a pushed block)
//   EffectKind17_TakeCell .. _CellBlocked 0x46B380 .. 0x46B6D0  their helpers (E8)
//   EffectKind1B_Start .. _Trail 0x46B7C0 .. 0x46B9A0  EffectKind1B_States 0..3
//   EffectKind1B_Hit           0x46BA90  EffectKind1B_Fly's test (E8)
//   EffectKind30_Run / _Start  0x46BB30 / 0x46BB50  Effect_KindHandlers[0x30], EffectKind30_States[0]
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. Sprite_Current
// is read again wherever the original reads [0x937F88] again after a call, and
// kept where it keeps it in a register. No divergence: each is a faithful
// replacement. Where the original would jump through a state table to what is
// not code, write through an index past Sprite_Objects / Sprite_ObjectsExtra /
// Effect_Objects / ObjTrio, divide by 0, or read its own stack frame, ours
// aborts with a message (docs/field_c1.md section 7); reads of the image's
// tables past their ends are kept (they read what the original reads).
#include "game/field_c1.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/config_text.h"
#include "game/field_c1_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = field_c1::at;
using move_script::At;
using move_script::Long;
using move_script::Word;
using U = std::uint32_t;
using Handler = void (__cdecl*)();

unsigned char* Cur() { return Sprite_Current; }
U AddressOfFieldC1(const void* p) { return Low32(p); }
U L(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }
std::int32_t S32(U v) { return static_cast<std::int32_t>(v); }
short S16(U v) { return static_cast<short>(v & 0xFFFFu); }

// A state table's entry, read in place (the fuzz swaps the entries for
// recorders); past the table's code the original jumps to what is not code.
Handler Entry(U table, unsigned index, const char* who) {
    const U cell = table + 4u * index;
    const U entry = L(move_script::At(cell));
    // A code word: hosted a VA in BOF3.exe's .text, in bof3x.exe a function of ours
    // the bind table names (common/image_base.h IsCodeWord); not the trap, not data.
    if (!scenario_harness::g_active && !::bof3::IsCodeWord(entry))
        bof3::Fatal("%s: index %u reads 0x%X at 0x%X (VA 0x%X), not code - past its table (the original jumps there)", who,
                    index, (unsigned)entry, (unsigned)cell, (unsigned)::bof3::VaOf(cell));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(entry));
}

// The records the handlers write by an index byte the original never checks.
unsigned char* SpriteRec(int i, const char* who) {
    if (i < 0 || i >= static_cast<int>(field_c1::at::kSpriteCount))
        bof3::Fatal("%s: Sprite_Objects index %d - the original writes outside the records", who, i);
    return rv::Bytes(Sprite_Objects) + static_cast<U>(i) * field_c1::at::kRecordStride;
}
unsigned char* ExtraRec(int i, const char* who) {
    if (i < 0 || i >= static_cast<int>(field_c1::at::kExtraCount))
        bof3::Fatal("%s: Sprite_ObjectsExtra index %d - the original writes outside the records", who, i);
    return rv::Bytes(Sprite_ObjectsExtra) + static_cast<U>(i) * field_c1::at::kRecordStride;
}

void Tick() {
    SH_CALL(Sprite_ScriptTick)();
    SH_CALL(Sprite_UpdateScreenSlot)();
}

// A byte of the area block's cell words (AreaMap_Header's word +2 the offset,
// in pairs of words): 0 marks a cell with no floor. The original indexes with
// the signed cell coordinates, unchecked; read in place.
U CellWord(short cx, short cz) {
    const int width = static_cast<int>(AreaMap_Header[0]);
    const int index = width * cz + static_cast<int>(Word(At(::bof3::Img(AreaMap_Header_addr) + 0x2))) * 2 + cx;
    return Word(At(AddressOfFieldC1(AreaMap_Header) + static_cast<U>(index) * 2u));
}

// The object at the effect's point (Sprite_ObjectAt's index): its +0x80 bit 0
// set - Sprite_Objects below 0x1E, Sprite_ObjectsExtra from there (the index
// a signed byte, as the original's movsx).
void MarkObject(unsigned char found, const char* who) {
    const int i = static_cast<signed char>(found);
    unsigned char* const rec = i < 0x1E ? SpriteRec(i, who) : ExtraRec(i - 0x1E, who);
    rec[0x80] = static_cast<unsigned char>(rec[0x80] | 1);
}

}  // namespace

// ============================================================================
// The Config screen's row label (START.EMI's resident draw)
// ============================================================================

// ============================================================================
// Effect kinds 6 and 0x19: two ticks of kind 6's, kind 0x19's run and tick
// ============================================================================

// ============================================================================
// Kind 4: Effect_HoldFlag1C's countdown
// ============================================================================

// ============================================================================
// Kind 0x31: the camera turn with a zoom
// ============================================================================

// ============================================================================
// Kind 0x32: a thing thrown from a sprite, bouncing
// ============================================================================

// ============================================================================
// Kind 0x37: an animation played through
// ============================================================================

// ============================================================================
// Kinds 0x14 and 0x3C: two tinted sprites for a while
// ============================================================================

// ============================================================================
// Kind 0x17: a block pushed along the map
// ============================================================================

// ============================================================================
// Kind 0x1B: a shot along the leader's facing, with a trail
// ============================================================================

// ============================================================================
// Kind 0x30
// ============================================================================

void FieldC1_Inject() {
    if (bof3::WantsShadow("field_c1")) field_c1::SelfTest();
}

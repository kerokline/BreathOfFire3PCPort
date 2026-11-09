// World 1's areas 53, 55..57 and 59..64: the PSX's BIN/WORLD01/AREA053..064.EMI
// compiled into the exe at 0x40AB00..0x40B8C0 (Area_Descriptors entries 53..64;
// areas 54 and 58 have no code). Round ten, group AR1D: the band's 47
// functions, none ours before, each read to its last instruction with
// capstone (2026-09-28) and taken through the area harness (area_harness.h).
// docs/area_w1d.md.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// two dispatchers through an area's .data state table abort past the table
// where the original would jump through whatever the next dword holds (the
// owner's rule for an unchecked index, round9 doc section 6; nothing measured
// reaches it). Reads by an unchecked index into the areas' .data tables are
// kept (they stay in .data). Every call goes through the harness (AH_CALL /
// AH_AT), so the start-up fuzz can stand recorders in for ours as for the
// originals' copies.
#include "game/area_w1d.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1d_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w1d::at;
using area_harness::Mem;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

using Handler = void (__cdecl*)();

unsigned char& B(std::uint32_t address) { return *Mem(address); }
void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is data).
Handler StateEntry(const char* who, std::uint32_t table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + index * 4u)))));
}

// The message box's answer, and the message word a choice leaves.
unsigned char Answer() { return B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)); }
void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }
// A message and the mark 0x9398CF = 6 beside it (areas 55, 59, 61).
void MessageMarked(unsigned id) {
    SetMessage(id);
    B(::bof3::Img(AreaW0c_AnswerMark_addr)) = 6;
}
// The byte a table indexed by the s8 answer holds (`movsx eax, answer`).
std::uint32_t BySignedAnswer(std::uint32_t table, unsigned stride, unsigned offset) {
    const auto answer = static_cast<signed char>(Answer());
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(table) + answer * static_cast<int>(stride) + static_cast<int>(offset));
}

// The pointer 0x903804 (a field object), read afresh at each use.
unsigned char* Focus() {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(::bof3::Img(AreaW1a_FocusObject_addr))))));
}

// Areas 55, 61's object triggers 23 / 21 and area 59's 24 begin so:
// ScriptFlags_Set40, the mode tail kind 4.
void Tail4(unsigned char sub) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 4;
    B(::bof3::Img(AreaW0c_TailSub_addr)) = sub;
}

// Areas 56 and 57's object triggers 52 / 53 (0x40B1D0, 0x40B2F0): story flag
// `flag` set; when flags 0x6D..0x70 are all set, flag 0x71 too (area 34's
// trigger 51 is the same with 0x6D). Answers 0 in al.
unsigned char FlagsGather(unsigned flag) {
    AH_CALL(Flags_Set)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), flag);
    for (unsigned f = 0x6D; f <= 0x70; ++f)
        if (!AH_CALL(Flags_Test)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), f)) return 0;
    AH_CALL(Flags_Set)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x71);
    return 0;
}

// ---- area 56 ----

// ---- areas 57, 59 ----

// ---- areas 63, 64 ----

// Areas 63 and 64's inits (0x40B720, 0x40B7F0): Rand & 0x3F walked through
// eight weights (the first whose weight exceeds what is left; none: 8);
// field objects 0..7 other than that one get byte +0 = 0; the one chosen (if
// any) is put at one of eight cells by Rand & 7 - x and z the pair's bytes
// << 16, its word +0x3E the ground's elevation there (x read back). Either
// way Field_StepCount = the leader's dword +0x134 less 5.
void PlaceOneObject(std::uint32_t positions, std::uint32_t weights) {
    auto left = static_cast<unsigned char>(AH_CALL(Rand)() & 0x3F);
    unsigned pick = 0;
    for (; pick < 8; ++pick) {
        const unsigned char w = B(weights + pick);
        if (left < w) break;
        left = static_cast<unsigned char>(left - w);
    }
    for (unsigned k = 0; k < 8; ++k)
        if (k != pick) Sprite_Objects[k].flags = 0;
    if (pick < 8) {
        const unsigned r = static_cast<unsigned char>(AH_CALL(Rand)() & 7);
        unsigned char* const object = rv::Bytes(Sprite_Objects) + pick * at::kObjectStride;
        auto* const tObject = rv::As<records::SpriteObject>(object);
        const std::int32_t z = static_cast<std::int32_t>(static_cast<std::uint32_t>(B(positions + r * 2 + 1)) << 16);
        const std::int32_t x = static_cast<std::int32_t>(static_cast<std::uint32_t>(B(positions + r * 2)) << 16);
        tObject->x = x;
        tObject->z = z;
        const long ground = AH_CALL(AreaMap_Elevation)(tObject->x, z);
        rv::Store<unsigned>(rv::As<records::SpriteObject>(object)->height_whole, static_cast<unsigned>(ground));
    }
    Field_StepCount = static_cast<unsigned short>(Long(Mem(::bof3::Img(ObjTrio_addr) + 0x134)) - 5);
}

}  // namespace

// ===========================================================================
// Area 53 (descriptor 0x5FEA98): choice 0 (choice 1, handler 0 and the init
// are 0x437CC0, a shared `ret` of another block), and object trigger 42.
// ===========================================================================

// ===========================================================================
// Area 55 (descriptor 0x6007D8): choices 0, 3 and 4 (1 and 2 are 0x420850 /
// 0x420870, another block's), object trigger 23. No handler, no init.
// ===========================================================================

// ===========================================================================
// Area 56 (descriptor 0x600BA0): sixteen choices, the last also handler 0
// (the fall through Area56_FallStates), object trigger 52. The init is
// 0x437CC0, a shared `ret`.
// ===========================================================================

// ===========================================================================
// Area 57 (descriptor 0x602928): choice 0 (also handler 4), handlers 0..3,
// object trigger 53. No init.
// ===========================================================================

// ===========================================================================
// Area 59 (descriptor 0x603C98): choices 0, 3 and 4 (1 and 2 other blocks'),
// handlers 2 and 3 (0 and 1 other blocks'), the step hook, object trigger 24,
// its effect (Effect_KindHandlers entry 0xB6) and the effect's two states.
// ===========================================================================

// ===========================================================================
// Area 60 (descriptor 0x603DE0): the init only.
// ===========================================================================

// ===========================================================================
// Area 61 (descriptor 0x6040B8): choices 0, 3 and 4 (1 and 2 other blocks'),
// object trigger 21.
// ===========================================================================

// ===========================================================================
// Area 62 (descriptor 0x6041E0): handler 0 arming the mode tail kind 0x2A.
// ===========================================================================

// ===========================================================================
// Areas 63 and 64 (descriptors 0x6042B0, 0x604380; one PSX descriptor
// address, two overlays): the inits only.
// ===========================================================================

void AreaW1d_Inject() {
    if (bof3::WantsShadow("area_w1d")) area_w1d::SelfTest();
}
